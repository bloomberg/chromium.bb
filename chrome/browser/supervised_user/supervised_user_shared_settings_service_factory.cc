// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_shared_settings_service_factory.h"

#include "chrome/browser/supervised_user/supervised_user_shared_settings_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

// static
SupervisedUserSharedSettingsService*
SupervisedUserSharedSettingsServiceFactory::GetForBrowserContext(
    content::BrowserContext* profile) {
  return static_cast<SupervisedUserSharedSettingsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SupervisedUserSharedSettingsServiceFactory*
SupervisedUserSharedSettingsServiceFactory::GetInstance() {
  return Singleton<SupervisedUserSharedSettingsServiceFactory>::get();
}

SupervisedUserSharedSettingsServiceFactory::
    SupervisedUserSharedSettingsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SupervisedUserSharedSettingsService",
          BrowserContextDependencyManager::GetInstance()) {}

SupervisedUserSharedSettingsServiceFactory::
    ~SupervisedUserSharedSettingsServiceFactory() {}

KeyedService*
SupervisedUserSharedSettingsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new SupervisedUserSharedSettingsService(
      user_prefs::UserPrefs::Get(profile));
}
