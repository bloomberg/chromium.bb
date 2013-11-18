// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/runtime/runtime_api_factory.h"

#include "chrome/browser/extensions/api/runtime/runtime_api.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

namespace extensions {

// static
RuntimeAPI* RuntimeAPIFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<RuntimeAPI*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
RuntimeAPIFactory* RuntimeAPIFactory::GetInstance() {
  return Singleton<RuntimeAPIFactory>::get();
}

RuntimeAPIFactory::RuntimeAPIFactory()
    : BrowserContextKeyedServiceFactory(
        "RuntimeAPI",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionSystemFactory::GetInstance());
}

RuntimeAPIFactory::~RuntimeAPIFactory() {
}

BrowserContextKeyedService* RuntimeAPIFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new RuntimeAPI(context);
}

content::BrowserContext* RuntimeAPIFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Instance is shared between incognito and regular profiles.
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool RuntimeAPIFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool RuntimeAPIFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
