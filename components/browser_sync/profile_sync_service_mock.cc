// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/profile_sync_service_mock.h"

#include <utility>

#include "components/sync/base/sync_prefs.h"

namespace browser_sync {

ProfileSyncServiceMock::ProfileSyncServiceMock(InitParams init_params)
    : ProfileSyncService(std::move(init_params)) {}

ProfileSyncServiceMock::~ProfileSyncServiceMock() {}

SyncUserSettingsMock* ProfileSyncServiceMock::GetUserSettingsMock() {
  return &user_settings_;
}

syncer::SyncUserSettings* ProfileSyncServiceMock::GetUserSettings() {
  return &user_settings_;
}

const syncer::SyncUserSettings* ProfileSyncServiceMock::GetUserSettings()
    const {
  return &user_settings_;
}

bool ProfileSyncServiceMock::IsAuthenticatedAccountPrimary() const {
  return true;
}

syncer::ModelTypeSet ProfileSyncServiceMock::GetPreferredDataTypes() const {
  return syncer::SyncPrefs::ResolvePrefGroups(
      /*registered_types=*/syncer::ModelTypeSet::All(),
      user_settings_.GetChosenDataTypes());
}

bool ProfileSyncServiceMock::IsPassphraseRequiredForDecryption() const {
  return user_settings_.IsPassphraseRequiredForDecryption();
}

bool ProfileSyncServiceMock::IsUsingSecondaryPassphrase() const {
  return user_settings_.IsUsingSecondaryPassphrase();
}

bool ProfileSyncServiceMock::IsPassphraseRequired() const {
  return user_settings_.IsPassphraseRequired();
}

std::unique_ptr<syncer::SyncSetupInProgressHandle>
ProfileSyncServiceMock::GetSetupInProgressHandleConcrete() {
  return browser_sync::ProfileSyncService::GetSetupInProgressHandle();
}

}  // namespace browser_sync
