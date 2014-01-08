// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_user_shared_settings_service_factory.h"

#include "chrome/browser/managed_mode/managed_user_shared_settings_service.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

// static
ManagedUserSharedSettingsService*
ManagedUserSharedSettingsServiceFactory::GetForBrowserContext(
    content::BrowserContext* profile) {
  return static_cast<ManagedUserSharedSettingsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ManagedUserSharedSettingsServiceFactory*
ManagedUserSharedSettingsServiceFactory::GetInstance() {
  return Singleton<ManagedUserSharedSettingsServiceFactory>::get();
}

ManagedUserSharedSettingsServiceFactory::
    ManagedUserSharedSettingsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ManagedUserSharedSettingsService",
          BrowserContextDependencyManager::GetInstance()) {}

ManagedUserSharedSettingsServiceFactory::
    ~ManagedUserSharedSettingsServiceFactory() {}

BrowserContextKeyedService*
ManagedUserSharedSettingsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new ManagedUserSharedSettingsService(
      user_prefs::UserPrefs::Get(profile));
}
