// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_sync_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_sync_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
SupervisedUserSyncService* SupervisedUserSyncServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SupervisedUserSyncService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SupervisedUserSyncServiceFactory*
SupervisedUserSyncServiceFactory::GetInstance() {
  return Singleton<SupervisedUserSyncServiceFactory>::get();
}

SupervisedUserSyncServiceFactory::SupervisedUserSyncServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SupervisedUserSyncService",
          BrowserContextDependencyManager::GetInstance()) {
}

SupervisedUserSyncServiceFactory::~SupervisedUserSyncServiceFactory() {}

KeyedService* SupervisedUserSyncServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new SupervisedUserSyncService(
      static_cast<Profile*>(profile)->GetPrefs());
}
