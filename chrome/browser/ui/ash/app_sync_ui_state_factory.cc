// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/app_sync_ui_state_factory.h"

#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/ash/app_sync_ui_state.h"

// static
AppSyncUIState* AppSyncUIStateFactory::GetForProfile(Profile* profile) {
  if (!AppSyncUIState::ShouldObserveAppSyncForProfile(profile))
    return NULL;

  return static_cast<AppSyncUIState*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
AppSyncUIStateFactory* AppSyncUIStateFactory::GetInstance() {
  return Singleton<AppSyncUIStateFactory>::get();
}

AppSyncUIStateFactory::AppSyncUIStateFactory()
    : ProfileKeyedServiceFactory("AppSyncUIState",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(ProfileSyncServiceFactory::GetInstance());
}

AppSyncUIStateFactory::~AppSyncUIStateFactory() {
}

ProfileKeyedService* AppSyncUIStateFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  DCHECK(AppSyncUIState::ShouldObserveAppSyncForProfile(profile));
  return new AppSyncUIState(profile);
}
