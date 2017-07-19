// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/proximity_auth_pref_manager.h"

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/values.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/proximity_auth/logging/logging.h"
#include "components/proximity_auth/proximity_auth_pref_names.h"

namespace proximity_auth {

ProximityAuthPrefManager::ProximityAuthPrefManager(PrefService* pref_service)
    : pref_service_(pref_service) {}

ProximityAuthPrefManager::~ProximityAuthPrefManager() {}

// static
void ProximityAuthPrefManager::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterInt64Pref(prefs::kProximityAuthLastPasswordEntryTimestampMs,
                              0L);
  registry->RegisterInt64Pref(
      prefs::kProximityAuthLastPromotionCheckTimestampMs, 0L);
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

bool ProximityAuthPrefManager::HasDeviceWithAddress(
    const std::string& bluetooth_address) const {
  return pref_service_->GetDictionary(prefs::kProximityAuthRemoteBleDevices)
      ->HasKey(bluetooth_address);
}

bool ProximityAuthPrefManager::HasDeviceWithPublicKey(
    const std::string& public_key) const {
  return !GetDeviceAddress(public_key).empty();
}

std::string ProximityAuthPrefManager::GetDevicePublicKey(
    const std::string& bluetooth_address) const {
  std::string public_key;
  GetRemoteBleDevices()->GetStringWithoutPathExpansion(bluetooth_address,
                                                       &public_key);
  return public_key;
}

std::string ProximityAuthPrefManager::GetDeviceAddress(
    const std::string& public_key) const {
  const base::DictionaryValue* remote_ble_devices = GetRemoteBleDevices();
  for (base::DictionaryValue::Iterator it(*remote_ble_devices); !it.IsAtEnd();
       it.Advance()) {
    std::string value_string;
    DCHECK(it.value().IsType(base::Value::Type::STRING));
    if (it.value().GetAsString(&value_string) && value_string == public_key)
      return it.key();
  }
  return std::string();
}

std::vector<std::string> ProximityAuthPrefManager::GetPublicKeys() const {
  std::vector<std::string> public_keys;
  const base::DictionaryValue* remote_ble_devices = GetRemoteBleDevices();
  for (base::DictionaryValue::Iterator it(*remote_ble_devices); !it.IsAtEnd();
       it.Advance()) {
    std::string value_string;
    DCHECK(it.value().IsType(base::Value::Type::STRING));
    it.value().GetAsString(&value_string);
    public_keys.push_back(value_string);
  }
  return public_keys;
}

void ProximityAuthPrefManager::AddOrUpdateDevice(
    const std::string& bluetooth_address,
    const std::string& public_key) {
  PA_LOG(INFO) << "Adding " << public_key << " , " << bluetooth_address
               << " pair.";
  if (HasDeviceWithPublicKey(public_key) &&
      GetDeviceAddress(public_key) != bluetooth_address) {
    PA_LOG(WARNING) << "Two devices with different bluetooth address, but the "
                       "same public key were added: "
                    << public_key;
    RemoveDeviceWithPublicKey(public_key);
  }

  DictionaryPrefUpdate remote_ble_devices_update(
      pref_service_, prefs::kProximityAuthRemoteBleDevices);
  remote_ble_devices_update->SetStringWithoutPathExpansion(bluetooth_address,
                                                           public_key);
}

bool ProximityAuthPrefManager::RemoveDeviceWithAddress(
    const std::string& bluetooth_address) {
  DictionaryPrefUpdate remote_ble_devices_update(
      pref_service_, prefs::kProximityAuthRemoteBleDevices);
  return remote_ble_devices_update->RemoveWithoutPathExpansion(
      bluetooth_address, nullptr);
}

bool ProximityAuthPrefManager::RemoveDeviceWithPublicKey(
    const std::string& public_key) {
  return RemoveDeviceWithAddress(GetDeviceAddress(public_key));
}
void ProximityAuthPrefManager::SetLastPasswordEntryTimestampMs(
    int64_t timestamp_ms) {
  pref_service_->SetInt64(prefs::kProximityAuthLastPasswordEntryTimestampMs,
                          timestamp_ms);
}

int64_t ProximityAuthPrefManager::GetLastPasswordEntryTimestampMs() const {
  return pref_service_->GetInt64(
      prefs::kProximityAuthLastPasswordEntryTimestampMs);
}

const base::DictionaryValue* ProximityAuthPrefManager::GetRemoteBleDevices()
    const {
  return pref_service_->GetDictionary(prefs::kProximityAuthRemoteBleDevices);
}

void ProximityAuthPrefManager::SetLastPromotionCheckTimestampMs(
    int64_t timestamp_ms) {
  pref_service_->SetInt64(prefs::kProximityAuthLastPasswordEntryTimestampMs,
                          timestamp_ms);
}

int64_t ProximityAuthPrefManager::GetLastPromotionCheckTimestampMs() const {
  return pref_service_->GetInt64(
      prefs::kProximityAuthLastPasswordEntryTimestampMs);
}

void ProximityAuthPrefManager::SetProximityThreshold(ProximityThreshold value) {
  pref_service_->SetInteger(prefs::kEasyUnlockProximityThreshold, value);
}

ProximityAuthPrefManager::ProximityThreshold
ProximityAuthPrefManager::GetProximityThreshold() const {
  int pref_value =
      pref_service_->GetInteger(prefs::kEasyUnlockProximityThreshold);
  return static_cast<ProximityThreshold>(pref_value);
}

void ProximityAuthPrefManager::SetIsChromeOSLoginEnabled(bool is_enabled) {
  return pref_service_->SetBoolean(prefs::kProximityAuthIsChromeOSLoginEnabled,
                                   is_enabled);
}

bool ProximityAuthPrefManager::IsChromeOSLoginEnabled() {
  return pref_service_->GetBoolean(prefs::kProximityAuthIsChromeOSLoginEnabled);
}

}  // namespace proximity_auth
