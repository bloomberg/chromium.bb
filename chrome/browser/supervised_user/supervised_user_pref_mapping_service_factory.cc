// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_pref_mapping_service_factory.h"

#include "chrome/browser/supervised_user/supervised_user_pref_mapping_service.h"
#include "chrome/browser/supervised_user/supervised_user_shared_settings_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

// static
SupervisedUserPrefMappingService*
SupervisedUserPrefMappingServiceFactory::GetForBrowserContext(
    content::BrowserContext* profile) {
  return static_cast<SupervisedUserPrefMappingService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SupervisedUserPrefMappingServiceFactory*
SupervisedUserPrefMappingServiceFactory::GetInstance() {
  return Singleton<SupervisedUserPrefMappingServiceFactory>::get();
}

SupervisedUserPrefMappingServiceFactory::
    SupervisedUserPrefMappingServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SupervisedUserPrefMappingService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(SupervisedUserSharedSettingsServiceFactory::GetInstance());
}

SupervisedUserPrefMappingServiceFactory::
    ~SupervisedUserPrefMappingServiceFactory() {}

KeyedService* SupervisedUserPrefMappingServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new SupervisedUserPrefMappingService(
      user_prefs::UserPrefs::Get(profile),
      SupervisedUserSharedSettingsServiceFactory::GetForBrowserContext(
          profile));
}
