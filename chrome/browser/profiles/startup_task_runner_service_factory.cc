// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/startup_task_runner_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/startup_task_runner_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

StartupTaskRunnerServiceFactory::StartupTaskRunnerServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "StartupTaskRunnerServiceFactory",
        BrowserContextDependencyManager::GetInstance()) {
}

StartupTaskRunnerServiceFactory::~StartupTaskRunnerServiceFactory() {}

// static
StartupTaskRunnerService* StartupTaskRunnerServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<StartupTaskRunnerService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
StartupTaskRunnerServiceFactory*
    StartupTaskRunnerServiceFactory::GetInstance() {
  return Singleton<StartupTaskRunnerServiceFactory>::get();
}

KeyedService* StartupTaskRunnerServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new StartupTaskRunnerService(static_cast<Profile*>(profile));
}
