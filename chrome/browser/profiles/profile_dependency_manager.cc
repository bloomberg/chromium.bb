// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_dependency_manager.h"

#include <algorithm>
#include <deque>
#include <iterator>

#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "content/common/notification_service.h"

class Profile;

void ProfileDependencyManager::AddComponent(
    ProfileKeyedServiceFactory* component) {
  all_components_.push_back(component);
  destruction_order_.clear();
}

void ProfileDependencyManager::RemoveComponent(
    ProfileKeyedServiceFactory* component) {
  all_components_.erase(std::remove(all_components_.begin(),
                                    all_components_.end(),
                                    component),
                        all_components_.end());

  // Remove all dependency edges that contain this component.
  EdgeMap::iterator it = edges_.begin();
  while (it != edges_.end()) {
    EdgeMap::iterator temp = it;
    ++it;

    if (temp->first == component || temp->second == component)
      edges_.erase(temp);
  }

  destruction_order_.clear();
}

void ProfileDependencyManager::AddEdge(ProfileKeyedServiceFactory* depended,
                                       ProfileKeyedServiceFactory* dependee) {
  edges_.insert(std::make_pair(depended, dependee));
  destruction_order_.clear();
}

void ProfileDependencyManager::DestroyProfileServices(Profile* profile) {
  if (destruction_order_.empty())
    BuildDestructionOrder();

  for (std::vector<ProfileKeyedServiceFactory*>::const_iterator it =
           destruction_order_.begin(); it != destruction_order_.end(); ++it) {
    (*it)->ProfileShutdown(profile);
  }

  for (std::vector<ProfileKeyedServiceFactory*>::const_iterator it =
           destruction_order_.begin(); it != destruction_order_.end(); ++it) {
    (*it)->ProfileDestroyed(profile);
  }
}

// static
ProfileDependencyManager* ProfileDependencyManager::GetInstance() {
  return Singleton<ProfileDependencyManager>::get();
}

ProfileDependencyManager::ProfileDependencyManager() {}

ProfileDependencyManager::~ProfileDependencyManager() {}

void ProfileDependencyManager::BuildDestructionOrder() {
  // Step 1: Build a set of nodes with no incoming edges.
  std::deque<ProfileKeyedServiceFactory*> queue;
  std::copy(all_components_.begin(),
            all_components_.end(),
            std::back_inserter(queue));

  std::deque<ProfileKeyedServiceFactory*>::iterator queue_end = queue.end();
  for (EdgeMap::const_iterator it = edges_.begin();
       it != edges_.end(); ++it) {
    queue_end = std::remove(queue.begin(), queue_end, it->second);
  }
  queue.erase(queue_end, queue.end());

  // Step 2: Do the Kahn topological sort.
  std::vector<ProfileKeyedServiceFactory*> output;
  EdgeMap edges(edges_);
  while (!queue.empty()) {
    ProfileKeyedServiceFactory* node = queue.front();
    queue.pop_front();
    output.push_back(node);

    std::pair<EdgeMap::iterator, EdgeMap::iterator> range =
        edges.equal_range(node);
    EdgeMap::iterator it = range.first;
    while (it != range.second) {
      ProfileKeyedServiceFactory* dest = it->second;
      EdgeMap::iterator temp = it;
      it++;
      edges.erase(temp);

      bool has_incoming_edges = false;
      for (EdgeMap::iterator jt = edges.begin(); jt != edges.end(); ++jt) {
        if (jt->second == dest) {
          has_incoming_edges = true;
          break;
        }
      }

      if (!has_incoming_edges)
        queue.push_back(dest);
    }
  }

  if (edges.size()) {
    NOTREACHED() << "Dependency graph has a cycle. We are doomed.";
  }

  std::reverse(output.begin(), output.end());
  destruction_order_ = output;
}
