// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/proximity_auth_pref_manager.h"

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/proximity_auth/logging/logging.h"
#include "components/proximity_auth/proximity_auth_pref_names.h"

namespace proximity_auth {

ProximityAuthPrefManager::ProximityAuthPrefManager(PrefService* pref_service)
    : pref_service_(pref_service) {}

ProximityAuthPrefManager::~ProximityAuthPrefManager() {}

// static
void ProximityAuthPrefManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kProximityAuthRemoteBleDevices);
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
    DCHECK(it.value().IsType(base::Value::TYPE_STRING));
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
    DCHECK(it.value().IsType(base::Value::TYPE_STRING));
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

const base::DictionaryValue* ProximityAuthPrefManager::GetRemoteBleDevices()
    const {
  return pref_service_->GetDictionary(prefs::kProximityAuthRemoteBleDevices);
}

}  // namespace proximity_auth
