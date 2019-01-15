// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/sync_user_settings_impl.h"

#include "components/browser_sync/profile_sync_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/sync_prefs.h"

namespace browser_sync {

SyncUserSettingsImpl::SyncUserSettingsImpl(ProfileSyncService* service,
                                           syncer::SyncPrefs* prefs)
    : service_(service),
      prefs_(prefs),
      registered_types_(service_->GetRegisteredDataTypes()) {
  DCHECK(service_);
  DCHECK(prefs_);
}

SyncUserSettingsImpl::~SyncUserSettingsImpl() = default;

bool SyncUserSettingsImpl::IsSyncRequested() const {
  return prefs_->IsSyncRequested();
}

void SyncUserSettingsImpl::SetSyncRequested(bool requested) {
  // TODO(crbug.com/884159): Write to prefs directly.
  if (requested) {
    service_->RequestStart();
  } else {
    service_->RequestStop(ProfileSyncService::KEEP_DATA);
  }
}

bool SyncUserSettingsImpl::IsSyncAllowedByPlatform() const {
  return sync_allowed_by_platform_;
}

void SyncUserSettingsImpl::SetSyncAllowedByPlatform(bool allowed) {
  if (sync_allowed_by_platform_ == allowed) {
    return;
  }

  sync_allowed_by_platform_ = allowed;

  service_->SyncAllowedByPlatformChanged(sync_allowed_by_platform_);
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
  // TODO(crbug.com/884159): Use SyncServiceCrypto.
  // Note: This requires *Profile*SyncService.
  return service_->IsEncryptEverythingAllowed();
}

void SyncUserSettingsImpl::SetEncryptEverythingAllowed(bool allowed) {
  // TODO(crbug.com/884159): Use SyncServiceCrypto.
  // Note: This requires *Profile*SyncService.
  service_->SetEncryptEverythingAllowed(allowed);
}

bool SyncUserSettingsImpl::IsEncryptEverythingEnabled() const {
  // TODO(crbug.com/884159): Use SyncServiceCrypto.
  return service_->IsEncryptEverythingEnabled();
}

void SyncUserSettingsImpl::EnableEncryptEverything() {
  // TODO(crbug.com/884159): Use SyncServiceCrypto.
  service_->EnableEncryptEverything();
}

bool SyncUserSettingsImpl::IsPassphraseRequired() const {
  // TODO(crbug.com/884159): Use SyncServiceCrypto.
  return service_->IsPassphraseRequired();
}

bool SyncUserSettingsImpl::IsPassphraseRequiredForDecryption() const {
  // TODO(crbug.com/884159): Use SyncServiceCrypto.
  return service_->IsPassphraseRequiredForDecryption();
}

bool SyncUserSettingsImpl::IsUsingSecondaryPassphrase() const {
  // TODO(crbug.com/884159): Use SyncServiceCrypto.
  return service_->IsUsingSecondaryPassphrase();
}

base::Time SyncUserSettingsImpl::GetExplicitPassphraseTime() const {
  // TODO(crbug.com/884159): Use SyncServiceCrypto.
  return service_->GetExplicitPassphraseTime();
}

syncer::PassphraseType SyncUserSettingsImpl::GetPassphraseType() const {
  // TODO(crbug.com/884159): Use SyncServiceCrypto.
  // Note: This requires *Profile*SyncService.
  return service_->GetPassphraseType();
}

void SyncUserSettingsImpl::SetEncryptionPassphrase(
    const std::string& passphrase) {
  // TODO(crbug.com/884159): Use SyncServiceCrypto.
  service_->SetEncryptionPassphrase(passphrase);
}

bool SyncUserSettingsImpl::SetDecryptionPassphrase(
    const std::string& passphrase) {
  // TODO(crbug.com/884159): Use SyncServiceCrypto.
  return service_->SetDecryptionPassphrase(passphrase);
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

}  // namespace browser_sync
