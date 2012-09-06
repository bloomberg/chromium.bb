// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/commands/command_service_factory.h"

#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace extensions {

// static
CommandService* CommandServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<CommandService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
CommandServiceFactory* CommandServiceFactory::GetInstance() {
  return Singleton<CommandServiceFactory>::get();
}

bool CommandServiceFactory::ServiceIsCreatedWithProfile() const {
  return true;
}

CommandServiceFactory::CommandServiceFactory()
    : ProfileKeyedServiceFactory("CommandService",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(ExtensionSystemFactory::GetInstance());
}

CommandServiceFactory::~CommandServiceFactory() {
}

ProfileKeyedService* CommandServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new CommandService(profile);
}

bool CommandServiceFactory::ServiceRedirectedInIncognito() const {
  return true;
}

}  // namespace extensions
