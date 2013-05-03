// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/developer_private_api_factory.h"

#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace extensions {

// static
DeveloperPrivateAPI* DeveloperPrivateAPIFactory::GetForProfile(
    Profile* profile) {
  return static_cast<DeveloperPrivateAPI*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
DeveloperPrivateAPIFactory* DeveloperPrivateAPIFactory::GetInstance() {
  return Singleton<DeveloperPrivateAPIFactory>::get();
}

DeveloperPrivateAPIFactory::DeveloperPrivateAPIFactory()
    : ProfileKeyedServiceFactory("DeveloperPrivateAPI",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(ExtensionSystemFactory::GetInstance());
}

DeveloperPrivateAPIFactory::~DeveloperPrivateAPIFactory() {
}

ProfileKeyedService* DeveloperPrivateAPIFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new DeveloperPrivateAPI(static_cast<Profile*>(profile));
}

content::BrowserContext* DeveloperPrivateAPIFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool DeveloperPrivateAPIFactory::ServiceIsCreatedWithProfile() const {
  return true;
}

bool DeveloperPrivateAPIFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
