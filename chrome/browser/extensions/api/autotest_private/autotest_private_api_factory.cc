// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autotest_private/autotest_private_api_factory.h"

#include "chrome/browser/extensions/api/autotest_private/autotest_private_api.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

// static
AutotestPrivateAPI* AutotestPrivateAPIFactory::GetForProfile(Profile* profile) {
  return static_cast<AutotestPrivateAPI*>(GetInstance()->
      GetServiceForBrowserContext(profile, true));
}

// static
AutotestPrivateAPIFactory* AutotestPrivateAPIFactory::GetInstance() {
  return Singleton<AutotestPrivateAPIFactory>::get();
}

AutotestPrivateAPIFactory::AutotestPrivateAPIFactory()
    : BrowserContextKeyedServiceFactory(
        "AutotestPrivateAPI",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionSystemFactory::GetInstance());
}

AutotestPrivateAPIFactory::~AutotestPrivateAPIFactory() {
}

BrowserContextKeyedService* AutotestPrivateAPIFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new AutotestPrivateAPI();
}

content::BrowserContext* AutotestPrivateAPIFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return ExtensionsBrowserClient::Get()->GetOriginalContext(context);
}

bool AutotestPrivateAPIFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool AutotestPrivateAPIFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
