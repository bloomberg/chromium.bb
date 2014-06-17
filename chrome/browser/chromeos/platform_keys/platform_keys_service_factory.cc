// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/platform_keys_service_factory.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {

// static
PlatformKeysService* PlatformKeysServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PlatformKeysService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
PlatformKeysServiceFactory* PlatformKeysServiceFactory::GetInstance() {
  return Singleton<PlatformKeysServiceFactory>::get();
}

PlatformKeysServiceFactory::PlatformKeysServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PlatformKeysService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
}

PlatformKeysServiceFactory::~PlatformKeysServiceFactory() {
}

content::BrowserContext* PlatformKeysServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* PlatformKeysServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  extensions::StateStore* store =
      extensions::ExtensionSystem::Get(context)->state_store();
  DCHECK(store);
  return new PlatformKeysService(context, store);
}

}  // namespace chromeos
