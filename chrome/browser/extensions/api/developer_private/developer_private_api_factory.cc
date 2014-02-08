// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/developer_private_api_factory.h"

#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

// static
DeveloperPrivateAPI* DeveloperPrivateAPIFactory::GetForProfile(
    Profile* profile) {
  return static_cast<DeveloperPrivateAPI*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
DeveloperPrivateAPIFactory* DeveloperPrivateAPIFactory::GetInstance() {
  return Singleton<DeveloperPrivateAPIFactory>::get();
}

DeveloperPrivateAPIFactory::DeveloperPrivateAPIFactory()
    : BrowserContextKeyedServiceFactory(
        "DeveloperPrivateAPI",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionSystemFactory::GetInstance());
}

DeveloperPrivateAPIFactory::~DeveloperPrivateAPIFactory() {
}

BrowserContextKeyedService* DeveloperPrivateAPIFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new DeveloperPrivateAPI(static_cast<Profile*>(profile));
}

content::BrowserContext* DeveloperPrivateAPIFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return ExtensionsBrowserClient::Get()->GetOriginalContext(context);
}

bool DeveloperPrivateAPIFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool DeveloperPrivateAPIFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
