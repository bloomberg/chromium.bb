// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_BLE_BLUETOOTH_LOW_ENERGY_WHITELIST_H
#define COMPONENTS_PROXIMITY_AUTH_BLE_BLUETOOTH_LOW_ENERGY_WHITELIST_H

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class DictionaryValue;
}  // namespace base

namespace proximity_auth {

// This class stores a persistent set of whitelisted bluetooth devices
// represented by pairs (bluetooth_address, public_key). The class enforces a
// bijective mapping: no two device share the same bluetooth_address or
// the same public_key.
class BluetoothLowEnergyDeviceWhitelist {
 public:
  // Creates a device whitelist backed by preferences registered in
  // |pref_service| (persistent across browser restarts). |pref_service| should
  // have been registered using RegisterPrefs(). Not owned, must out live this
  // instance.
  explicit BluetoothLowEnergyDeviceWhitelist(PrefService* pref_service);
  virtual ~BluetoothLowEnergyDeviceWhitelist();

  // Registers the prefs used by this class to the given |pref_service|.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Virtual for testing
  virtual bool HasDeviceWithAddress(const std::string& bluetooth_address) const;
  bool HasDeviceWithPublicKey(const std::string& public_key) const;

  std::string GetDevicePublicKey(const std::string& bluetooth_address) const;
  std::string GetDeviceAddress(const std::string& public_key) const;
  std::vector<std::string> GetPublicKeys() const;

  // Adds a device with |bluetooth_address| and |public_key|. The
  // bluetooth_address <-> public_key mapping is unique, i.e. if there is
  // another device with same bluetooth address or public key that device will
  // be replaced.
  void AddOrUpdateDevice(const std::string& bluetooth_address,
                         const std::string& public_key);

  // Removes the unique device with |bluetooth_address| (|public_key|). Returns
  // false if no device was found.
  bool RemoveDeviceWithAddress(const std::string& bluetooth_address);
  bool RemoveDeviceWithPublicKey(const std::string& public_key);

 private:
  const base::DictionaryValue* GetWhitelistPrefs() const;

  // Contains perferences that outlive the lifetime of this object and across
  // process restarts.
  // Not owned and must outlive this instance.
  PrefService* pref_service_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothLowEnergyDeviceWhitelist);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_BLE_BLUETOOTH_WHITELIST_H
