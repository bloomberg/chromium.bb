// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/sync_user_settings_impl.h"

#include "base/metrics/histogram_macros.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/sync_service_crypto.h"

namespace browser_sync {

SyncUserSettingsImpl::SyncUserSettingsImpl(
    syncer::SyncServiceCrypto* crypto,
    syncer::SyncPrefs* prefs,
    syncer::ModelTypeSet registered_types,
    const base::RepeatingCallback<void(bool)>& sync_allowed_by_platform_changed)
    : crypto_(crypto),
      prefs_(prefs),
      registered_types_(registered_types),
      sync_allowed_by_platform_changed_cb_(sync_allowed_by_platform_changed) {
  DCHECK(crypto_);
  DCHECK(prefs_);
}

SyncUserSettingsImpl::~SyncUserSettingsImpl() = default;

bool SyncUserSettingsImpl::IsSyncRequested() const {
  return prefs_->IsSyncRequested();
}

void SyncUserSettingsImpl::SetSyncRequested(bool requested) {
  prefs_->SetSyncRequested(requested);
}

bool SyncUserSettingsImpl::IsSyncAllowedByPlatform() const {
  return sync_allowed_by_platform_;
}

void SyncUserSettingsImpl::SetSyncAllowedByPlatform(bool allowed) {
  if (sync_allowed_by_platform_ == allowed) {
    return;
  }

  sync_allowed_by_platform_ = allowed;

  sync_allowed_by_platform_changed_cb_.Run(sync_allowed_by_platform_);
}

bool SyncUserSettingsImpl::IsFirstSetupComplete() const {
  return prefs_->IsFirstSetupComplete();
}

void SyncUserSettingsImpl::SetFirstSetupComplete() {
  prefs_->SetFirstSetupComplete();
}

bool SyncUserSettingsImpl::IsSyncEverythingEnabled() const {
  return prefs_->HasKeepEverythingSynced();
}

syncer::ModelTypeSet SyncUserSettingsImpl::GetChosenDataTypes() const {
  syncer::ModelTypeSet types = GetPreferredDataTypes();
  types.RetainAll(syncer::UserSelectableTypes());
  return types;
}

void SyncUserSettingsImpl::SetChosenDataTypes(bool sync_everything,
                                              syncer::ModelTypeSet types) {
  DCHECK(syncer::UserSelectableTypes().HasAll(types));

  prefs_->SetDataTypesConfiguration(sync_everything, registered_types_, types);
}

bool SyncUserSettingsImpl::IsEncryptEverythingAllowed() const {
  return crypto_->IsEncryptEverythingAllowed();
}

void SyncUserSettingsImpl::SetEncryptEverythingAllowed(bool allowed) {
  crypto_->SetEncryptEverythingAllowed(allowed);
}

bool SyncUserSettingsImpl::IsEncryptEverythingEnabled() const {
  return crypto_->IsEncryptEverythingEnabled();
}

void SyncUserSettingsImpl::EnableEncryptEverything() {
  crypto_->EnableEncryptEverything();
}

bool SyncUserSettingsImpl::IsPassphraseRequired() const {
  return crypto_->passphrase_required_reason() !=
         syncer::REASON_PASSPHRASE_NOT_REQUIRED;
}

bool SyncUserSettingsImpl::IsPassphraseRequiredForDecryption() const {
  // If there is an encrypted datatype enabled and we don't have the proper
  // passphrase, we must prompt the user for a passphrase. The only way for the
  // user to avoid entering their passphrase is to disable the encrypted types.
  return IsEncryptedDatatypeEnabled() && IsPassphraseRequired();
}

bool SyncUserSettingsImpl::IsUsingSecondaryPassphrase() const {
  return crypto_->IsUsingSecondaryPassphrase();
}

base::Time SyncUserSettingsImpl::GetExplicitPassphraseTime() const {
  return crypto_->GetExplicitPassphraseTime();
}

syncer::PassphraseType SyncUserSettingsImpl::GetPassphraseType() const {
  return crypto_->GetPassphraseType();
}

void SyncUserSettingsImpl::SetEncryptionPassphrase(
    const std::string& passphrase) {
  crypto_->SetEncryptionPassphrase(passphrase);
}

bool SyncUserSettingsImpl::SetDecryptionPassphrase(
    const std::string& passphrase) {
  DCHECK(IsPassphraseRequired())
      << "SetDecryptionPassphrase must not be called when "
         "IsPassphraseRequired() is false.";

  DVLOG(1) << "Setting passphrase for decryption.";

  bool result = crypto_->SetDecryptionPassphrase(passphrase);
  UMA_HISTOGRAM_BOOLEAN("Sync.PassphraseDecryptionSucceeded", result);
  return result;
}

syncer::ModelTypeSet SyncUserSettingsImpl::GetPreferredDataTypes() const {
  syncer::ModelTypeSet types = Union(
      prefs_->GetPreferredDataTypes(registered_types_), syncer::ControlTypes());
  if (prefs_->IsLocalSyncEnabled()) {
    types.Remove(syncer::APP_LIST);
    types.Remove(syncer::USER_CONSENTS);
    types.Remove(syncer::USER_EVENTS);
  }
  return types;
}

syncer::ModelTypeSet SyncUserSettingsImpl::GetEncryptedDataTypes() const {
  return crypto_->GetEncryptedDataTypes();
}

bool SyncUserSettingsImpl::IsEncryptedDatatypeEnabled() const {
  if (IsEncryptionPending())
    return true;
  const syncer::ModelTypeSet preferred_types = GetPreferredDataTypes();
  const syncer::ModelTypeSet encrypted_types = GetEncryptedDataTypes();
  DCHECK(encrypted_types.Has(syncer::PASSWORDS));
  return !Intersection(preferred_types, encrypted_types).Empty();
}

bool SyncUserSettingsImpl::IsEncryptionPending() const {
  return crypto_->encryption_pending();
}

}  // namespace browser_sync
