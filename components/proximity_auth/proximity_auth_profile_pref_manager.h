// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_PROFILE_PREF_MANAGER_H
#define COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_PROFILE_PREF_MANAGER_H

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/proximity_auth/proximity_auth_pref_manager.h"

class PrefService;

namespace base {
class DictionaryValue;
}  // namespace base

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace proximity_auth {

// Implementation of ProximityAuthPrefManager for a logged in session with a
// user profile.
class ProximityAuthProfilePrefManager : public ProximityAuthPrefManager {
 public:
  // Creates a pref manager backed by preferences registered in
  // |pref_service| (persistent across browser restarts). |pref_service| should
  // have been registered using RegisterPrefs(). Not owned, must out live this
  // instance.
  explicit ProximityAuthProfilePrefManager(PrefService* pref_service);
  ~ProximityAuthProfilePrefManager() override;

  // Registers the prefs used by this class to the given |pref_service|.
  static void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry);

  // ProximityAuthPrefManager:
  void AddOrUpdateDevice(const std::string& bluetooth_address,
                         const std::string& public_key) override;
  bool RemoveDeviceWithAddress(const std::string& bluetooth_address) override;
  bool RemoveDeviceWithPublicKey(const std::string& public_key) override;
  bool HasDeviceWithAddress(
      const std::string& bluetooth_address) const override;
  bool HasDeviceWithPublicKey(const std::string& public_key) const override;
  std::string GetDevicePublicKey(
      const std::string& bluetooth_address) const override;
  std::string GetDeviceAddress(const std::string& public_key) const override;
  std::vector<std::string> GetPublicKeys() const override;
  void SetLastPasswordEntryTimestampMs(int64_t timestamp_ms) override;
  int64_t GetLastPasswordEntryTimestampMs() const override;
  void SetLastPromotionCheckTimestampMs(int64_t timestamp_ms) override;
  int64_t GetLastPromotionCheckTimestampMs() const override;
  void SetPromotionShownCount(int count) override;
  int GetPromotionShownCount() const override;
  void SetProximityThreshold(ProximityThreshold value) override;
  ProximityThreshold GetProximityThreshold() const override;
  void SetIsChromeOSLoginEnabled(bool is_enabled) override;
  bool IsChromeOSLoginEnabled() override;

 private:
  const base::DictionaryValue* GetRemoteBleDevices() const;

  // Contains perferences that outlive the lifetime of this object and across
  // process restarts. Not owned and must outlive this instance.
  PrefService* pref_service_;

  DISALLOW_COPY_AND_ASSIGN(ProximityAuthProfilePrefManager);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_PROFILE_PREF_MANAGER_H
