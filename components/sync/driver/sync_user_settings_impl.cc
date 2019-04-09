// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_user_settings_impl.h"

#include "base/metrics/histogram_macros.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/sync_service_crypto.h"

namespace syncer {

namespace {

ModelTypeSet ResolvePrefGroups(ModelTypeSet chosen_types) {
  DCHECK(UserSelectableTypes().HasAll(chosen_types));
  ModelTypeSet types_with_groups = chosen_types;
  if (chosen_types.Has(APPS)) {
    types_with_groups.PutAll({APP_SETTINGS, APP_LIST, ARC_PACKAGE});
  }
  if (chosen_types.Has(AUTOFILL)) {
    types_with_groups.PutAll(
        {AUTOFILL_PROFILE, AUTOFILL_WALLET_DATA, AUTOFILL_WALLET_METADATA});
  }
  if (chosen_types.Has(EXTENSIONS)) {
    types_with_groups.Put(EXTENSION_SETTINGS);
  }
  if (chosen_types.Has(PREFERENCES)) {
    types_with_groups.PutAll(
        {DICTIONARY, PRIORITY_PREFERENCES, SEARCH_ENGINES});
  }
  if (chosen_types.Has(TYPED_URLS)) {
    types_with_groups.PutAll({HISTORY_DELETE_DIRECTIVES, SESSIONS,
                              FAVICON_IMAGES, FAVICON_TRACKING, USER_EVENTS});
  }
  if (chosen_types.Has(PROXY_TABS)) {
    types_with_groups.PutAll(
        {SESSIONS, FAVICON_IMAGES, FAVICON_TRACKING, SEND_TAB_TO_SELF});
  }

  return types_with_groups;
}

}  // namespace

SyncUserSettingsImpl::SyncUserSettingsImpl(
    SyncServiceCrypto* crypto,
    SyncPrefs* prefs,
    ModelTypeSet registered_types,
    const base::RepeatingCallback<void(bool)>& sync_allowed_by_platform_changed,
    const base::RepeatingCallback<bool()>& is_encrypt_everything_allowed)
    : crypto_(crypto),
      prefs_(prefs),
      registered_types_(registered_types),
      sync_allowed_by_platform_changed_cb_(sync_allowed_by_platform_changed),
      is_encrypt_everything_allowed_cb_(is_encrypt_everything_allowed) {
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

ModelTypeSet SyncUserSettingsImpl::GetChosenDataTypes() const {
  ModelTypeSet types = prefs_->GetChosenDataTypes();
  DCHECK(UserSelectableTypes().HasAll(types));
  types.RetainAll(registered_types_);
  return types;
}

void SyncUserSettingsImpl::SetChosenDataTypes(bool sync_everything,
                                              ModelTypeSet types) {
  DCHECK(UserSelectableTypes().HasAll(types));
  DCHECK(registered_types_.HasAll(types));
  prefs_->SetDataTypesConfiguration(
      sync_everything,
      /*choosable_types=*/
      Intersection(registered_types_, UserSelectableTypes()),
      /*chosen_types=*/Intersection(registered_types_, types));
}

bool SyncUserSettingsImpl::IsEncryptEverythingAllowed() const {
  return is_encrypt_everything_allowed_cb_.Run();
}

bool SyncUserSettingsImpl::IsEncryptEverythingEnabled() const {
  return crypto_->IsEncryptEverythingEnabled();
}

void SyncUserSettingsImpl::EnableEncryptEverything() {
  DCHECK(IsEncryptEverythingAllowed());
  crypto_->EnableEncryptEverything();
}

bool SyncUserSettingsImpl::IsPassphraseRequired() const {
  return crypto_->passphrase_required_reason() !=
         REASON_PASSPHRASE_NOT_REQUIRED;
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

PassphraseType SyncUserSettingsImpl::GetPassphraseType() const {
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

void SyncUserSettingsImpl::SetSyncRequestedIfNotSetExplicitly() {
  prefs_->SetSyncRequestedIfNotSetExplicitly();
}

ModelTypeSet SyncUserSettingsImpl::GetPreferredDataTypes() const {
  ModelTypeSet types;
  if (IsSyncEverythingEnabled()) {
    // TODO(crbug.com/950874): it's possible to remove this case if we accept
    // behavioral change. When one of UserSelectableTypes() isn't registered,
    // but one of its corresponding UserTypes() is registered, current
    // implementation treats that corresponding type as preferred while
    // implementation without processing of this case won't treat that type
    // as preferred.
    types = registered_types_;
  } else {
    types = ResolvePrefGroups(GetChosenDataTypes());
    types.PutAll(AlwaysPreferredUserTypes());
    types.RetainAll(registered_types_);
  }

  types.PutAll(ControlTypes());
  if (prefs_->IsLocalSyncEnabled()) {
    types.Remove(APP_LIST);
    types.Remove(USER_CONSENTS);
    types.Remove(USER_EVENTS);
  }
  return types;
}

ModelTypeSet SyncUserSettingsImpl::GetEncryptedDataTypes() const {
  return crypto_->GetEncryptedDataTypes();
}

bool SyncUserSettingsImpl::IsEncryptedDatatypeEnabled() const {
  if (IsEncryptionPending())
    return true;
  const ModelTypeSet preferred_types = GetPreferredDataTypes();
  const ModelTypeSet encrypted_types = GetEncryptedDataTypes();
  DCHECK(encrypted_types.Has(PASSWORDS));
  return !Intersection(preferred_types, encrypted_types).Empty();
}

bool SyncUserSettingsImpl::IsEncryptionPending() const {
  return crypto_->encryption_pending();
}

// static
ModelTypeSet SyncUserSettingsImpl::ResolvePrefGroupsForTesting(
    ModelTypeSet chosen_types) {
  return ResolvePrefGroups(chosen_types);
}

}  // namespace syncer
