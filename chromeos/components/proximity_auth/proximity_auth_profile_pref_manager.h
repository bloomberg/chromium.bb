// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_PROFILE_PREF_MANAGER_H_
#define CHROMEOS_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_PROFILE_PREF_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/proximity_auth/proximity_auth_pref_manager.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/core/account_id/account_id.h"

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

  // Initializes the manager to listen to pref changes and sync prefs to the
  // user's local state.
  void StartSyncingToLocalState(PrefService* local_state,
                                const AccountId& account_id);

  // Registers the prefs used by this class to the given |pref_service|.
  static void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry);

  // ProximityAuthPrefManager:
  bool IsEasyUnlockAllowed() const override;
  void SetIsEasyUnlockEnabled(bool is_easy_unlock_enabled) const override;
  bool IsEasyUnlockEnabled() const override;
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

  void SyncPrefsToLocalState();

  // Contains perferences that outlive the lifetime of this object and across
  // process restarts. Not owned and must outlive this instance.
  PrefService* pref_service_;

  // Listens to pref changes so they can be synced to the local state.
  PrefChangeRegistrar registrar_;

  // The local state to which to sync the profile prefs.
  PrefService* local_state_;

  // The account id of the current profile.
  AccountId account_id_;

  base::WeakPtrFactory<ProximityAuthProfilePrefManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProximityAuthProfilePrefManager);
};

}  // namespace proximity_auth

#endif  // CHROMEOS_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_PROFILE_PREF_MANAGER_H_
