// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/ble/bluetooth_low_energy_device_whitelist.h"

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/proximity_auth/ble/pref_names.h"
#include "components/proximity_auth/logging/logging.h"

namespace proximity_auth {

BluetoothLowEnergyDeviceWhitelist::BluetoothLowEnergyDeviceWhitelist(
    PrefService* pref_service)
    : pref_service_(pref_service) {
}

BluetoothLowEnergyDeviceWhitelist::~BluetoothLowEnergyDeviceWhitelist() {
}

// static
void BluetoothLowEnergyDeviceWhitelist::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kBluetoothLowEnergyDeviceWhitelist);
}

bool BluetoothLowEnergyDeviceWhitelist::HasDeviceWithAddress(
    const std::string& bluetooth_address) const {
  return pref_service_->GetDictionary(prefs::kBluetoothLowEnergyDeviceWhitelist)
      ->HasKey(bluetooth_address);
}

bool BluetoothLowEnergyDeviceWhitelist::HasDeviceWithPublicKey(
    const std::string& public_key) const {
  return !GetDeviceAddress(public_key).empty();
}

std::string BluetoothLowEnergyDeviceWhitelist::GetDevicePublicKey(
    const std::string& bluetooth_address) const {
  std::string public_key;
  GetWhitelistPrefs()->GetStringWithoutPathExpansion(bluetooth_address,
                                                     &public_key);
  return public_key;
}

std::string BluetoothLowEnergyDeviceWhitelist::GetDeviceAddress(
    const std::string& public_key) const {
  const base::DictionaryValue* device_whitelist = GetWhitelistPrefs();
  for (base::DictionaryValue::Iterator it(*device_whitelist); !it.IsAtEnd();
       it.Advance()) {
    std::string value_string;
    DCHECK(it.value().IsType(base::Value::TYPE_STRING));
    if (it.value().GetAsString(&value_string) && value_string == public_key)
      return it.key();
  }
  return std::string();
}

std::vector<std::string> BluetoothLowEnergyDeviceWhitelist::GetPublicKeys()
    const {
  std::vector<std::string> public_keys;
  const base::DictionaryValue* device_whitelist = GetWhitelistPrefs();
  for (base::DictionaryValue::Iterator it(*device_whitelist); !it.IsAtEnd();
       it.Advance()) {
    std::string value_string;
    DCHECK(it.value().IsType(base::Value::TYPE_STRING));
    it.value().GetAsString(&value_string);
    public_keys.push_back(value_string);
  }
  return public_keys;
}

void BluetoothLowEnergyDeviceWhitelist::AddOrUpdateDevice(
    const std::string& bluetooth_address,
    const std::string& public_key) {
  if (HasDeviceWithPublicKey(public_key) &&
      GetDeviceAddress(public_key) != bluetooth_address) {
    PA_LOG(WARNING) << "Two devices with different bluetooth address, but the "
                       "same public key were added: " << public_key;
    RemoveDeviceWithPublicKey(public_key);
  }

  DictionaryPrefUpdate device_whitelist_update(
      pref_service_, prefs::kBluetoothLowEnergyDeviceWhitelist);
  device_whitelist_update->SetStringWithoutPathExpansion(bluetooth_address,
                                                         public_key);
}

bool BluetoothLowEnergyDeviceWhitelist::RemoveDeviceWithAddress(
    const std::string& bluetooth_address) {
  DictionaryPrefUpdate device_whitelist_update(
      pref_service_, prefs::kBluetoothLowEnergyDeviceWhitelist);
  return device_whitelist_update->RemoveWithoutPathExpansion(bluetooth_address,
                                                             nullptr);
}

bool BluetoothLowEnergyDeviceWhitelist::RemoveDeviceWithPublicKey(
    const std::string& public_key) {
  return RemoveDeviceWithAddress(GetDeviceAddress(public_key));
}

const base::DictionaryValue*
BluetoothLowEnergyDeviceWhitelist::GetWhitelistPrefs() const {
  return pref_service_->GetDictionary(
      prefs::kBluetoothLowEnergyDeviceWhitelist);
}

}  // namespace proximity_auth
