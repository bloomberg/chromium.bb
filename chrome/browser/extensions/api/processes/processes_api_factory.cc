// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/processes/processes_api_factory.h"

#include "chrome/browser/extensions/api/processes/processes_api.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace extensions {

// static
ProcessesAPI* ProcessesAPIFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ProcessesAPI*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
ProcessesAPIFactory* ProcessesAPIFactory::GetInstance() {
  return Singleton<ProcessesAPIFactory>::get();
}

ProcessesAPIFactory::ProcessesAPIFactory()
    : ProfileKeyedServiceFactory("ProcessesAPI",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(ExtensionSystemFactory::GetInstance());
}

ProcessesAPIFactory::~ProcessesAPIFactory() {
}

ProfileKeyedService* ProcessesAPIFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new ProcessesAPI(profile);
}

bool ProcessesAPIFactory::ServiceRedirectedInIncognito() const {
  return true;
}

bool ProcessesAPIFactory::ServiceIsCreatedWithProfile() const {
  return true;
}

bool ProcessesAPIFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
