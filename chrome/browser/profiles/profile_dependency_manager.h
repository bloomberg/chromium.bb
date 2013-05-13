// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_DEPENDENCY_MANAGER_H_
#define CHROME_BROWSER_PROFILES_PROFILE_DEPENDENCY_MANAGER_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/dependency_graph.h"

#ifndef NDEBUG
#include <set>
#endif

class Profile;
class ProfileKeyedBaseFactory;

// A singleton that listens for profile destruction notifications and
// rebroadcasts them to each ProfileKeyedBaseFactory in a safe order based
// on the stated dependencies by each service.
class ProfileDependencyManager {
 public:
  // Adds/Removes a component from our list of live components. Removing will
  // also remove live dependency links.
  void AddComponent(ProfileKeyedBaseFactory* component);
  void RemoveComponent(ProfileKeyedBaseFactory* component);

  // Adds a dependency between two factories.
  void AddEdge(ProfileKeyedBaseFactory* depended,
               ProfileKeyedBaseFactory* dependee);

  // Called by each Profile to alert us of its creation. Several services want
  // to be started when a profile is created. Testing configuration is also
  // done at this time. (If you want your ProfileKeyedService to be started
  // with the Profile, override ProfileKeyedBaseFactory::
  // ServiceIsCreatedWithProfile() to return true.)
  void CreateProfileServices(Profile* profile, bool is_testing_profile);

  // Called by each Profile to alert us that we should destroy services
  // associated with it.
  //
  // Why not use the existing PROFILE_DESTROYED notification?
  //
  // - Because we need to do everything here after the application has handled
  //   being notified about PROFILE_DESTROYED.
  // - Because this class is a singleton and Singletons can't rely on
  //   NotificationService in unit tests because NotificationService is
  //   replaced in many tests.
  void DestroyProfileServices(Profile* profile);

#ifndef NDEBUG
  // Debugging assertion called as part of GetServiceForProfile in debug
  // mode. This will NOTREACHED() whenever the user is trying to access a stale
  // Profile*.
  void AssertProfileWasntDestroyed(Profile* profile);
#endif

  static ProfileDependencyManager* GetInstance();

 private:
  friend class ProfileDependencyManagerUnittests;
  friend struct DefaultSingletonTraits<ProfileDependencyManager>;

  ProfileDependencyManager();
  virtual ~ProfileDependencyManager();

  // Ensures that all the factories have been created before building the
  // dependency graph.
  void AssertFactoriesBuilt();

#ifndef NDEBUG
  void DumpProfileDependencies(Profile* profile);
#endif

  // Whether AssertFactoriesBuilt has been done.
  bool built_factories_;

  DependencyGraph dependency_graph_;

#ifndef NDEBUG
  // A list of profile objects that have gone through the Shutdown()
  // phase. These pointers are most likely invalid, but we keep track of their
  // locations in memory so we can nicely assert if we're asked to do anything
  // with them.
  std::set<Profile*> dead_profile_pointers_;
#endif
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_DEPENDENCY_MANAGER_H_
