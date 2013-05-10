// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_dependency_manager.h"

#include <algorithm>
#include <deque>
#include <iterator>

#include "base/bind.h"
#include "chrome/browser/profiles/profile_keyed_base_factory.h"
#include "content/public/browser/browser_context.h"

#ifndef NDEBUG
#include "base/command_line.h"
#include "base/file_util.h"
#include "chrome/common/chrome_switches.h"
#endif

class Profile;

void ProfileDependencyManager::AddComponent(
    ProfileKeyedBaseFactory* component) {
  dependency_graph_.AddNode(component);
}

void ProfileDependencyManager::RemoveComponent(
    ProfileKeyedBaseFactory* component) {
  dependency_graph_.RemoveNode(component);
}

void ProfileDependencyManager::AddEdge(ProfileKeyedBaseFactory* depended,
                                       ProfileKeyedBaseFactory* dependee) {
  dependency_graph_.AddEdge(depended, dependee);
}

void ProfileDependencyManager::CreateProfileServices(
    content::BrowserContext* profile, bool is_testing_profile) {
#ifndef NDEBUG
  // Unmark |profile| as dead. This exists because of unit tests, which will
  // often have similar stack structures. 0xWhatever might be created, go out
  // of scope, and then a new Profile object might be created at 0xWhatever.
  dead_profile_pointers_.erase(profile);
#endif

  std::vector<DependencyNode*> construction_order;
  if (!dependency_graph_.GetConstructionOrder(&construction_order)) {
    NOTREACHED();
  }

#ifndef NDEBUG
  DumpProfileDependencies(profile);
#endif

  for (size_t i = 0; i < construction_order.size(); i++) {
    ProfileKeyedBaseFactory* factory =
        static_cast<ProfileKeyedBaseFactory*>(construction_order[i]);

    if (!profile->IsOffTheRecord()) {
      // We only register preferences on normal profiles because the incognito
      // profile shares the pref service with the normal one.
      factory->RegisterUserPrefsOnProfile(profile);
    }

    if (is_testing_profile && factory->ServiceIsNULLWhileTesting()) {
      factory->SetEmptyTestingFactory(profile);
    } else if (factory->ServiceIsCreatedWithProfile()) {
      // Create the service.
      factory->CreateServiceNow(profile);
    }
  }
}

void ProfileDependencyManager::DestroyProfileServices(
    content::BrowserContext* profile) {
  std::vector<DependencyNode*> destruction_order;
  if (!dependency_graph_.GetDestructionOrder(&destruction_order)) {
    NOTREACHED();
  }

#ifndef NDEBUG
  DumpProfileDependencies(profile);
#endif

  for (size_t i = 0; i < destruction_order.size(); i++) {
    ProfileKeyedBaseFactory* factory =
        static_cast<ProfileKeyedBaseFactory*>(destruction_order[i]);
    factory->ProfileShutdown(profile);
  }

#ifndef NDEBUG
  // The profile is now dead to the rest of the program.
  dead_profile_pointers_.insert(profile);
#endif

  for (size_t i = 0; i < destruction_order.size(); i++) {
    ProfileKeyedBaseFactory* factory =
        static_cast<ProfileKeyedBaseFactory*>(destruction_order[i]);
    factory->ProfileDestroyed(profile);
  }
}

#ifndef NDEBUG
void ProfileDependencyManager::AssertProfileWasntDestroyed(
    content::BrowserContext* profile) {
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

ProfileDependencyManager::ProfileDependencyManager() {
}

ProfileDependencyManager::~ProfileDependencyManager() {
}

#ifndef NDEBUG
namespace {

std::string ProfileKeyedBaseFactoryGetNodeName(DependencyNode* node) {
  return static_cast<ProfileKeyedBaseFactory*>(node)->name();
}

}  // namespace

void ProfileDependencyManager::DumpProfileDependencies(
    content::BrowserContext* profile) {
  // Whenever we try to build a destruction ordering, we should also dump a
  // dependency graph to "/path/to/profile/profile-dependencies.dot".
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDumpProfileDependencyGraph)) {
    base::FilePath dot_file =
        profile->GetPath().AppendASCII("profile-dependencies.dot");
    std::string contents = dependency_graph_.DumpAsGraphviz(
        "Profile", base::Bind(&ProfileKeyedBaseFactoryGetNodeName));
    file_util::WriteFile(dot_file, contents.c_str(), contents.size());
  }
}
#endif  // NDEBUG
