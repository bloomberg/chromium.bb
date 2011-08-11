// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_dependency_manager.h"

#include <algorithm>
#include <deque>
#include <iterator>

#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/plugin_prefs.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service_factory.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "content/common/notification_service.h"

class Profile;

namespace {

bool g_initialized = false;

// This method gets the instance of each ServiceFactory. We do this so that
// each ServiceFactory initializes iteslf and registers its dependencies with
// the global PreferenceDependencyManager. We need to have a complete
// dependency graph when we create a profile so we can dispatch the profile
// creation message to the services that want to create their services at
// profile creation time.
//
// TODO(erg): This needs to be something else. I don't think putting every
// FooServiceFactory here will scale or is desireable long term.
void AssertFactoriesBuilt() {
  if (!g_initialized) {
    BackgroundContentsServiceFactory::GetInstance();
    CloudPrintProxyServiceFactory::GetInstance();
    PluginPrefs::Initialize();
    SessionServiceFactory::GetInstance();
    TabRestoreServiceFactory::GetInstance();
    TemplateURLServiceFactory::GetInstance();

    g_initialized = true;
  }
}

}  // namespace

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

void ProfileDependencyManager::CreateProfileServices(Profile* profile,
                                                     bool is_testing_profile) {
#ifndef NDEBUG
  // Unmark |profile| as dead. This exists because of unit tests, which will
  // often have similar stack structures. 0xWhatever might be created, go out
  // of scope, and then a new Profile object might be created at 0xWhatever.
  dead_profile_pointers_.erase(profile);
#endif

  AssertFactoriesBuilt();

  if (destruction_order_.empty())
    BuildDestructionOrder();

  // Iterate in reverse destruction order for creation.
  for (std::vector<ProfileKeyedServiceFactory*>::reverse_iterator rit =
           destruction_order_.rbegin(); rit != destruction_order_.rend();
       ++rit) {
    if (is_testing_profile && (*rit)->ServiceIsNULLWhileTesting()) {
      (*rit)->SetTestingFactory(profile, NULL);
    } else if ((*rit)->ServiceIsCreatedWithProfile()) {
      // Create the service.
      (*rit)->GetServiceForProfile(profile, true);
    }
  }
}

void ProfileDependencyManager::DestroyProfileServices(Profile* profile) {
  if (destruction_order_.empty())
    BuildDestructionOrder();

  for (std::vector<ProfileKeyedServiceFactory*>::const_iterator it =
           destruction_order_.begin(); it != destruction_order_.end(); ++it) {
    (*it)->ProfileShutdown(profile);
  }

#ifndef NDEBUG
  // The profile is now dead to the rest of the program.
  dead_profile_pointers_.insert(profile);
#endif

  for (std::vector<ProfileKeyedServiceFactory*>::const_iterator it =
           destruction_order_.begin(); it != destruction_order_.end(); ++it) {
    (*it)->ProfileDestroyed(profile);
  }
}

#ifndef NDEBUG
void ProfileDependencyManager::AssertProfileWasntDestroyed(Profile* profile) {
  if (dead_profile_pointers_.find(profile) != dead_profile_pointers_.end()) {
    NOTREACHED() << "Attempted to access a Profile that was ShutDown(). This "
                 << "is most likely a heap smasher in progress. After "
                 << "ProfileKeyedService::Shutdown() completes, your service "
                 << "MUST NOT refer to depended Profile services again.";
  }
}
#endif

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
