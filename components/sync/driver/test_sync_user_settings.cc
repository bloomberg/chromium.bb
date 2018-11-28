// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/test_sync_user_settings.h"

#include "components/sync/base/passphrase_enums.h"
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
  return service_->IsFirstSetupComplete();
}

void TestSyncUserSettings::SetFirstSetupComplete() {
  service_->SetFirstSetupComplete(true);
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
  service_->OnUserChoseDatatypes(sync_everything, types);
}

bool TestSyncUserSettings::IsEncryptEverythingAllowed() const {
  return true;
}

void TestSyncUserSettings::SetEncryptEverythingAllowed(bool allowed) {}

bool TestSyncUserSettings::IsEncryptEverythingEnabled() const {
  return service_->IsEncryptEverythingEnabled();
}

void TestSyncUserSettings::EnableEncryptEverything() {
  service_->EnableEncryptEverything();
}

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
  return service_->GetExplicitPassphraseTime();
}

PassphraseType TestSyncUserSettings::GetPassphraseType() const {
  return IsUsingSecondaryPassphrase() ? PassphraseType::CUSTOM_PASSPHRASE
                                      : PassphraseType::IMPLICIT_PASSPHRASE;
}

void TestSyncUserSettings::SetEncryptionPassphrase(
    const std::string& passphrase) {
  service_->SetEncryptionPassphrase(passphrase);
}

bool TestSyncUserSettings::SetDecryptionPassphrase(
    const std::string& passphrase) {
  return service_->SetDecryptionPassphrase(passphrase);
}

}  // namespace syncer
