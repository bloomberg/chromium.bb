// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry_factory.h"

#include "chrome/browser/extensions/api/discovery/suggested_links_registry.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

namespace extensions {

// static
TabCaptureRegistry* TabCaptureRegistryFactory::GetForProfile(Profile* profile) {
  return static_cast<TabCaptureRegistry*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
TabCaptureRegistryFactory* TabCaptureRegistryFactory::GetInstance() {
  return Singleton<TabCaptureRegistryFactory>::get();
}

bool TabCaptureRegistryFactory::ServiceIsCreatedWithBrowserContext() const {
  return false;
}

TabCaptureRegistryFactory::TabCaptureRegistryFactory()
    : BrowserContextKeyedServiceFactory(
        "TabCaptureRegistry",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionSystemFactory::GetInstance());
}

TabCaptureRegistryFactory::~TabCaptureRegistryFactory() {
}

BrowserContextKeyedService* TabCaptureRegistryFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new TabCaptureRegistry(static_cast<Profile*>(profile));
}

content::BrowserContext* TabCaptureRegistryFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace extensions
