// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_user_settings_service_factory.h"

#include "chrome/browser/managed_mode/managed_user_settings_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

// static
ManagedUserSettingsService*
ManagedUserSettingsServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<ManagedUserSettingsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ManagedUserSettingsServiceFactory*
ManagedUserSettingsServiceFactory::GetInstance() {
  return Singleton<ManagedUserSettingsServiceFactory>::get();
}

ManagedUserSettingsServiceFactory::ManagedUserSettingsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ManagedUserSettingsService",
          BrowserContextDependencyManager::GetInstance()) {
}

ManagedUserSettingsServiceFactory::
    ~ManagedUserSettingsServiceFactory() {}

BrowserContextKeyedService*
ManagedUserSettingsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new ManagedUserSettingsService();
}

content::BrowserContext*
ManagedUserSettingsServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}
