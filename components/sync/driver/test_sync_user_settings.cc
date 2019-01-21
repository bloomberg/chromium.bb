// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/test_sync_user_settings.h"

#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/test_sync_service.h"

namespace syncer {

TestSyncUserSettings::TestSyncUserSettings(TestSyncService* service)
    : service_(service) {}

TestSyncUserSettings::~TestSyncUserSettings() = default;

bool TestSyncUserSettings::IsSyncRequested() const {
  return !service_->HasDisableReason(SyncService::DISABLE_REASON_USER_CHOICE);
}

void TestSyncUserSettings::SetSyncRequested(bool requested) {
  int disable_reasons = service_->GetDisableReasons();
  if (requested) {
    disable_reasons &= ~SyncService::DISABLE_REASON_USER_CHOICE;
  } else {
    disable_reasons |= SyncService::DISABLE_REASON_USER_CHOICE;
  }
  service_->SetDisableReasons(disable_reasons);
}

bool TestSyncUserSettings::IsSyncAllowedByPlatform() const {
  return !service_->HasDisableReason(
      SyncService::DISABLE_REASON_PLATFORM_OVERRIDE);
}

void TestSyncUserSettings::SetSyncAllowedByPlatform(bool allowed) {
  int disable_reasons = service_->GetDisableReasons();
  if (allowed) {
    disable_reasons &= ~SyncService::DISABLE_REASON_PLATFORM_OVERRIDE;
  } else {
    disable_reasons |= SyncService::DISABLE_REASON_PLATFORM_OVERRIDE;
  }
  service_->SetDisableReasons(disable_reasons);
}

bool TestSyncUserSettings::IsFirstSetupComplete() const {
  return first_setup_complete_;
}

void TestSyncUserSettings::SetFirstSetupComplete() {
  SetFirstSetupComplete(true);
}

bool TestSyncUserSettings::IsSyncEverythingEnabled() const {
  return sync_everything_enabled_;
}

ModelTypeSet TestSyncUserSettings::GetChosenDataTypes() const {
  ModelTypeSet types = service_->GetPreferredDataTypes();
  types.RetainAll(UserSelectableTypes());
  return types;
}

void TestSyncUserSettings::SetChosenDataTypes(bool sync_everything,
                                              ModelTypeSet types) {
  sync_everything_enabled_ = sync_everything;
  syncer::ModelTypeSet preferred_types;
  if (sync_everything_enabled_) {
    preferred_types = syncer::ModelTypeSet::All();
  } else {
    preferred_types = syncer::SyncPrefs::ResolvePrefGroups(
        /*registered_types=*/syncer::ModelTypeSet::All(), types);
  }
  service_->SetPreferredDataTypes(preferred_types);
}

bool TestSyncUserSettings::IsEncryptEverythingAllowed() const {
  return true;
}

void TestSyncUserSettings::SetEncryptEverythingAllowed(bool allowed) {}

bool TestSyncUserSettings::IsEncryptEverythingEnabled() const {
  return false;
}

void TestSyncUserSettings::EnableEncryptEverything() {}

bool TestSyncUserSettings::IsPassphraseRequired() const {
  return service_->IsPassphraseRequired();
}

bool TestSyncUserSettings::IsPassphraseRequiredForDecryption() const {
  return service_->IsPassphraseRequiredForDecryption();
}

bool TestSyncUserSettings::IsUsingSecondaryPassphrase() const {
  return service_->IsUsingSecondaryPassphrase();
}

base::Time TestSyncUserSettings::GetExplicitPassphraseTime() const {
  return base::Time();
}

PassphraseType TestSyncUserSettings::GetPassphraseType() const {
  return IsUsingSecondaryPassphrase() ? PassphraseType::CUSTOM_PASSPHRASE
                                      : PassphraseType::IMPLICIT_PASSPHRASE;
}

void TestSyncUserSettings::SetEncryptionPassphrase(
    const std::string& passphrase) {}

bool TestSyncUserSettings::SetDecryptionPassphrase(
    const std::string& passphrase) {
  return false;
}

void TestSyncUserSettings::SetFirstSetupComplete(bool first_setup_complete) {
  first_setup_complete_ = first_setup_complete;
}

}  // namespace syncer
