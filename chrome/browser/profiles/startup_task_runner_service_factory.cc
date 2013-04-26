// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/startup_task_runner_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/profiles/startup_task_runner_service.h"

StartupTaskRunnerServiceFactory::StartupTaskRunnerServiceFactory()
    : ProfileKeyedServiceFactory("StartupTaskRunnerServiceFactory",
                                 ProfileDependencyManager::GetInstance()) {
}

StartupTaskRunnerServiceFactory::~StartupTaskRunnerServiceFactory() {}

// static
StartupTaskRunnerService* StartupTaskRunnerServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<StartupTaskRunnerService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
StartupTaskRunnerServiceFactory*
    StartupTaskRunnerServiceFactory::GetInstance() {
  return Singleton<StartupTaskRunnerServiceFactory>::get();
}

ProfileKeyedService* StartupTaskRunnerServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new StartupTaskRunnerService(static_cast<Profile*>(profile));
}
