// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_warning_badge_service_factory.h"

#include "chrome/browser/extensions/extension_warning_badge_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/warning_service_factory.h"

using content::BrowserContext;

namespace extensions {

// static
ExtensionWarningBadgeService*
ExtensionWarningBadgeServiceFactory::GetForBrowserContext(
    BrowserContext* context) {
  return static_cast<ExtensionWarningBadgeService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ExtensionWarningBadgeServiceFactory*
ExtensionWarningBadgeServiceFactory::GetInstance() {
  return Singleton<ExtensionWarningBadgeServiceFactory>::get();
}

ExtensionWarningBadgeServiceFactory::ExtensionWarningBadgeServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ExtensionWarningBadgeService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(WarningServiceFactory::GetInstance());
}

ExtensionWarningBadgeServiceFactory::~ExtensionWarningBadgeServiceFactory() {
}

KeyedService* ExtensionWarningBadgeServiceFactory::BuildServiceInstanceFor(
    BrowserContext* context) const {
  return new ExtensionWarningBadgeService(static_cast<Profile*>(context));
}

BrowserContext* ExtensionWarningBadgeServiceFactory::GetBrowserContextToUse(
    BrowserContext* context) const {
  // Redirected in incognito.
  return ExtensionsBrowserClient::Get()->GetOriginalContext(context);
}

bool ExtensionWarningBadgeServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace extensions
