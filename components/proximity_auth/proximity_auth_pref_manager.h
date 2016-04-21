// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_PREF_MANAGER_H
#define COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_PREF_MANAGER_H

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

// This class manages the local (persistent) settings used by Smart Lock. It
// provides methods to set and queries the local settings, e.g. the address of
// BLE devices.
class ProximityAuthPrefManager {
 public:
  // Creates a pref manager backed by preferences registered in
  // |pref_service| (persistent across browser restarts). |pref_service| should
  // have been registered using RegisterPrefs(). Not owned, must out live this
  // instance.
  explicit ProximityAuthPrefManager(PrefService* pref_service);
  virtual ~ProximityAuthPrefManager();

  // Registers the prefs used by this class to the given |pref_service|.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Methods used to handle remote BLE devices stored in prefs
  // (kProximityAuthRemoteBleDevices).
  //
  // Each device correspond to a (public_key, bluetooth_address) pair. The
  // bluetooth_address <-> public_key mapping is unique, i.e. if there is
  // another device with same bluetooth address or public key that device will
  // be replaced.
  void AddOrUpdateDevice(const std::string& bluetooth_address,
                         const std::string& public_key);

  // Removes the unique device with |bluetooth_address| (|public_key|). Returns
  // false if no device was found.
  bool RemoveDeviceWithAddress(const std::string& bluetooth_address);
  bool RemoveDeviceWithPublicKey(const std::string& public_key);

  // Queries the devices stored in |kProximityAuthRemoteBleDevices| pref by
  // |bluetooth_address| or |public_key|. Virtual for testing.
  virtual bool HasDeviceWithAddress(const std::string& bluetooth_address) const;
  virtual bool HasDeviceWithPublicKey(const std::string& public_key) const;
  virtual std::string GetDevicePublicKey(
      const std::string& bluetooth_address) const;
  virtual std::string GetDeviceAddress(const std::string& public_key) const;
  virtual std::vector<std::string> GetPublicKeys() const;

 private:
  const base::DictionaryValue* GetRemoteBleDevices() const;

  // Contains perferences that outlive the lifetime of this object and across
  // process restarts. Not owned and must outlive this instance.
  PrefService* pref_service_;

  DISALLOW_COPY_AND_ASSIGN(ProximityAuthPrefManager);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_PREF_MANAGER_H
