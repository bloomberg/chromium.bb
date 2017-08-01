// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/proximity_auth_profile_pref_manager.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/values.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/proximity_auth/logging/logging.h"
#include "components/proximity_auth/proximity_auth_pref_names.h"

namespace proximity_auth {

ProximityAuthProfilePrefManager::ProximityAuthProfilePrefManager(
    PrefService* pref_service)
    : pref_service_(pref_service), weak_ptr_factory_(this) {}

ProximityAuthProfilePrefManager::~ProximityAuthProfilePrefManager() {
  registrar_.RemoveAll();
}

// static
void ProximityAuthProfilePrefManager::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kEasyUnlockAllowed, true);
  registry->RegisterInt64Pref(prefs::kProximityAuthLastPasswordEntryTimestampMs,
                              0L);
  registry->RegisterInt64Pref(
      prefs::kProximityAuthLastPromotionCheckTimestampMs, 0L);
  registry->RegisterIntegerPref(prefs::kProximityAuthPromotionShownCount, 0);
  registry->RegisterDictionaryPref(prefs::kProximityAuthRemoteBleDevices);
  registry->RegisterIntegerPref(
      prefs::kEasyUnlockProximityThreshold, 1,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // TODO(tengs): For existing EasyUnlock users, we want to maintain their
  // current behaviour and keep login enabled. However, for new users, we will
  // disable login when setting up EasyUnlock.
  // After a sufficient number of releases, we should make the default value
  // false.
  registry->RegisterBooleanPref(
      prefs::kProximityAuthIsChromeOSLoginEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void ProximityAuthProfilePrefManager::StartSyncingToLocalState(
    PrefService* local_state,
    const AccountId& account_id) {
  local_state_ = local_state;
  account_id_ = account_id;

  if (!account_id_.is_valid()) {
    PA_LOG(ERROR) << "Invalid account_id.";
    return;
  }

  base::Closure on_pref_changed_callback =
      base::Bind(&ProximityAuthProfilePrefManager::SyncPrefsToLocalState,
                 weak_ptr_factory_.GetWeakPtr());

  registrar_.Init(pref_service_);
  registrar_.Add(prefs::kEasyUnlockAllowed, on_pref_changed_callback);
  registrar_.Add(proximity_auth::prefs::kEasyUnlockProximityThreshold,
                 on_pref_changed_callback);
  registrar_.Add(proximity_auth::prefs::kProximityAuthIsChromeOSLoginEnabled,
                 on_pref_changed_callback);

  SyncPrefsToLocalState();
}

void ProximityAuthProfilePrefManager::SyncPrefsToLocalState() {
  std::unique_ptr<base::DictionaryValue> user_prefs_dict(
      new base::DictionaryValue());

  user_prefs_dict->SetBooleanWithoutPathExpansion(prefs::kEasyUnlockAllowed,
                                                  IsEasyUnlockAllowed());
  user_prefs_dict->SetIntegerWithoutPathExpansion(
      prefs::kEasyUnlockProximityThreshold, GetProximityThreshold());
  user_prefs_dict->SetBooleanWithoutPathExpansion(
      prefs::kProximityAuthIsChromeOSLoginEnabled, IsChromeOSLoginEnabled());

  DictionaryPrefUpdate update(local_state_,
                              prefs::kEasyUnlockLocalStateUserPrefs);
  update->SetWithoutPathExpansion(account_id_.GetUserEmail(),
                                  std::move(user_prefs_dict));
}

bool ProximityAuthProfilePrefManager::IsEasyUnlockAllowed() const {
  return pref_service_->GetBoolean(prefs::kEasyUnlockAllowed);
}

void ProximityAuthProfilePrefManager::SetLastPasswordEntryTimestampMs(
    int64_t timestamp_ms) {
  pref_service_->SetInt64(prefs::kProximityAuthLastPasswordEntryTimestampMs,
                          timestamp_ms);
}

int64_t ProximityAuthProfilePrefManager::GetLastPasswordEntryTimestampMs()
    const {
  return pref_service_->GetInt64(
      prefs::kProximityAuthLastPasswordEntryTimestampMs);
}

void ProximityAuthProfilePrefManager::SetLastPromotionCheckTimestampMs(
    int64_t timestamp_ms) {
  pref_service_->SetInt64(prefs::kProximityAuthLastPromotionCheckTimestampMs,
                          timestamp_ms);
}

int64_t ProximityAuthProfilePrefManager::GetLastPromotionCheckTimestampMs()
    const {
  return pref_service_->GetInt64(
      prefs::kProximityAuthLastPromotionCheckTimestampMs);
}

void ProximityAuthProfilePrefManager::SetPromotionShownCount(int count) {
  pref_service_->SetInteger(prefs::kProximityAuthPromotionShownCount, count);
}

int ProximityAuthProfilePrefManager::GetPromotionShownCount() const {
  return pref_service_->GetInteger(prefs::kProximityAuthPromotionShownCount);
}

void ProximityAuthProfilePrefManager::SetProximityThreshold(
    ProximityThreshold value) {
  pref_service_->SetInteger(prefs::kEasyUnlockProximityThreshold, value);
}

ProximityAuthProfilePrefManager::ProximityThreshold
ProximityAuthProfilePrefManager::GetProximityThreshold() const {
  int pref_value =
      pref_service_->GetInteger(prefs::kEasyUnlockProximityThreshold);
  return static_cast<ProximityThreshold>(pref_value);
}

void ProximityAuthProfilePrefManager::SetIsChromeOSLoginEnabled(
    bool is_enabled) {
  return pref_service_->SetBoolean(prefs::kProximityAuthIsChromeOSLoginEnabled,
                                   is_enabled);
}

bool ProximityAuthProfilePrefManager::IsChromeOSLoginEnabled() {
  return pref_service_->GetBoolean(prefs::kProximityAuthIsChromeOSLoginEnabled);
}

}  // namespace proximity_auth
