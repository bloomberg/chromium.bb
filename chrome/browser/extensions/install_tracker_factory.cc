// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/install_tracker_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace extensions {

// static
InstallTracker* InstallTrackerFactory::GetForProfile(Profile* profile) {
  return static_cast<InstallTracker*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

InstallTrackerFactory* InstallTrackerFactory::GetInstance() {
  return Singleton<InstallTrackerFactory>::get();
}

InstallTrackerFactory::InstallTrackerFactory()
    : ProfileKeyedServiceFactory("InstallTracker",
                                 ProfileDependencyManager::GetInstance()) {
}

InstallTrackerFactory::~InstallTrackerFactory() {
}

ProfileKeyedService* InstallTrackerFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new InstallTracker();
}

bool InstallTrackerFactory::ServiceRedirectedInIncognito() const {
  // The installs themselves are routed to the non-incognito profile and so
  // should the install progress.
  return true;
}

}  // namespace extensions
