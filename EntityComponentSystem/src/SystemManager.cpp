/*
	Author : Tobias Stein
	Date   : 13th July, 2016
	File   : SystemManager.cpp
	
	Manager class for systems

	All Rights Reserved. (c) Copyright 2016.
*/

#include "SystemManager.h"
#include "ISystem.h"

namespace ECS
{
	SystemManager::SystemManager()
	{
		DEFINE_LOGGER("SystemManager")

		LogInfo("Initialize SystemManager!");

		// acquire global memory
		this->m_SystemAllocator = new SystemAllocator(SystemManager::SYSTEM_MEMORY_CAPACITY, Allocate(SystemManager::SYSTEM_MEMORY_CAPACITY, "SystemManager"));
	}
	
	SystemManager::~SystemManager()
	{
		for (auto system : this->m_Systems)
		{
			system.second->~ISystem();
			system.second = nullptr;
		}

		m_Systems.clear();

		// free allocated global memory
		Free((void*)this->m_SystemAllocator->GetMemoryAddress0());
		delete this->m_SystemAllocator;
		this->m_SystemAllocator = nullptr;
		
		LogInfo("Release SystemManager!");
	}

	void SystemManager::Update()
	{
		for (ISystem* system : this->m_SystemWorkOrder)
		{
			system->Tick(TIME_STEP);
		}
	}

	void SystemManager::UpdateSystemWorkOrder()
	{
		// depth-first-search function
		static std::function<void(int, std::vector<int>&, const std::vector<std::vector<bool>>&, std::vector<SystemTypeId>&)> DFS = [&](int vertex, std::vector<int>& VERTEX_STATE, const std::vector<std::vector<bool>>& EDGES, std::vector<SystemTypeId>& output)
		{
			VERTEX_STATE[vertex] = 1; // visited

			for (int i = 0; i < VERTEX_STATE.size(); ++i)
			{
				if (EDGES[i][vertex] == true && VERTEX_STATE[i] == 0)
					DFS(i, VERTEX_STATE, EDGES, output);
			}

			VERTEX_STATE[vertex] = 2; // done
			output.push_back(vertex);
		};

		const size_t NUM_SYSTEMS = this->m_SystemDependencyMatrix.size();


		// create index array
		std::vector<int> INDICES(NUM_SYSTEMS);
		for (int i = 0; i < NUM_SYSTEMS; ++i)
			INDICES[i] = i;

		// determine vertex-groups
		std::vector<std::vector<SystemTypeId>>	VERTEX_GROUPS;
		std::vector<SystemPriority>				GROUP_PRIORITY;

		while (INDICES.empty() == false)
		{
			int index = INDICES.back();
			INDICES.pop_back();

			if (index == -1)
				continue;

			std::vector<SystemTypeId> group;
			std::vector<SystemTypeId> member;

			SystemPriority groupPriority = LOWEST_SYSTEM_PRIORITY;
			member.push_back(index);

			while (member.empty() == false)
			{
				index = member.back();
				member.pop_back();

				for (int i = 0; i < INDICES.size(); ++i)
				{
					if (INDICES[i] != -1 && (this->m_SystemDependencyMatrix[i][index] == true || this->m_SystemDependencyMatrix[index][i] == true))
					{
						member.push_back(i);
						INDICES[i] = -1;
					}
				}

				group.push_back(index);
				groupPriority = std::max(this->m_Systems[index]->GetSystemPriority(), groupPriority);
			}

			VERTEX_GROUPS.push_back(group);
			GROUP_PRIORITY.push_back(groupPriority);
		}

		const int NUM_VERTEX_GROUPS = VERTEX_GROUPS.size();

		// do a topological sort on groups w.r.t. to groups priority!
		std::vector<int> vertex_states(NUM_SYSTEMS, 0);

		std::multimap<SystemPriority, std::vector<SystemTypeId>> VERTEX_GROUPS_SORTED;


		for (int i = 0; i < VERTEX_GROUPS.size(); ++i)
		{
			auto g = VERTEX_GROUPS[i];

			std::vector<SystemTypeId> order;

			for (int i = 0; i < g.size(); ++i)
			{
				if (vertex_states[g[i]] == 0)
					DFS(g[i], vertex_states, this->m_SystemDependencyMatrix, order);
			}

			std::reverse(order.begin(), order.end());
			
			VERTEX_GROUPS_SORTED.insert(std::pair<SystemPriority, std::vector<SystemTypeId>>(std::numeric_limits<SystemPriority>::max() - GROUP_PRIORITY[i], order));
		}



		LogInfo("Update system work order:")

		// re-build system work order
		this->m_SystemWorkOrder.clear();
		for (auto group : VERTEX_GROUPS_SORTED)
		{
			for (auto m : group.second)
			{
				this->m_SystemWorkOrder.push_back(this->m_Systems[m]);
				LogInfo("\t%s", this->m_Systems[m]->GetSystemTypeName())
			}
		}
	}

} // namespace ECS