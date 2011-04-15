// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_mode_manager_factory.h"

#include "base/command_line.h"
#include "chrome/browser/background_mode_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

// static
BackgroundModeManager* BackgroundModeManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<BackgroundModeManager*>(
      GetInstance()->GetServiceForProfile(profile));
}

// static
BackgroundModeManagerFactory* BackgroundModeManagerFactory::GetInstance() {
  return Singleton<BackgroundModeManagerFactory>::get();
}

BackgroundModeManagerFactory::BackgroundModeManagerFactory()
    : ProfileKeyedServiceFactory(ProfileDependencyManager::GetInstance()) {
}

BackgroundModeManagerFactory::~BackgroundModeManagerFactory() {
}

ProfileKeyedService* BackgroundModeManagerFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new BackgroundModeManager(profile, CommandLine::ForCurrentProcess());
}
