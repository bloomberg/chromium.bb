// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_SYNC_USER_SETTINGS_MOCK_H_
#define COMPONENTS_BROWSER_SYNC_SYNC_USER_SETTINGS_MOCK_H_

#include <string>

#include "components/sync/driver/sync_user_settings.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browser_sync {

class SyncUserSettingsMock : public syncer::SyncUserSettings {
 public:
  SyncUserSettingsMock();
  ~SyncUserSettingsMock() override;

  MOCK_CONST_METHOD0(IsSyncRequested, bool());
  MOCK_METHOD1(SetSyncRequested, void(bool));

  MOCK_CONST_METHOD0(IsSyncAllowedByPlatform, bool());
  MOCK_METHOD1(SetSyncAllowedByPlatform, void(bool));

  MOCK_CONST_METHOD0(IsFirstSetupComplete, bool());
  MOCK_METHOD0(SetFirstSetupComplete, void());

  MOCK_CONST_METHOD0(IsSyncEverythingEnabled, bool());
  MOCK_CONST_METHOD0(GetChosenDataTypes, syncer::ModelTypeSet());
  MOCK_METHOD2(SetChosenDataTypes, void(bool, syncer::ModelTypeSet));

  MOCK_CONST_METHOD0(IsEncryptEverythingAllowed, bool());
  MOCK_METHOD1(SetEncryptEverythingAllowed, void(bool));
  MOCK_CONST_METHOD0(IsEncryptEverythingEnabled, bool());
  MOCK_METHOD0(EnableEncryptEverything, void());

  MOCK_CONST_METHOD0(IsPassphraseRequired, bool());
  MOCK_CONST_METHOD0(IsPassphraseRequiredForDecryption, bool());
  MOCK_CONST_METHOD0(IsUsingSecondaryPassphrase, bool());
  MOCK_CONST_METHOD0(GetExplicitPassphraseTime, base::Time());
  MOCK_CONST_METHOD0(GetPassphraseType, syncer::PassphraseType());

  MOCK_METHOD1(SetEncryptionPassphrase, void(const std::string&));
  MOCK_METHOD1(SetDecryptionPassphrase, bool(const std::string&));
};

}  // namespace browser_sync

#endif  // COMPONENTS_BROWSER_SYNC_SYNC_USER_SETTINGS_MOCK_H_
