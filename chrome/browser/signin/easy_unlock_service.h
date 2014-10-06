// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/signin/easy_unlock_screenlock_state_handler.h"
#include "components/keyed_service/core/keyed_service.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_types.h"
#endif

namespace base {
class DictionaryValue;
class ListValue;
}

namespace user_manager {
class User;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class EasyUnlockAuthAttempt;
class EasyUnlockServiceObserver;
class Profile;
class PrefRegistrySimple;

class EasyUnlockService : public KeyedService {
 public:
  enum TurnOffFlowStatus {
    IDLE,
    PENDING,
    FAIL,
  };

  enum Type {
    TYPE_REGULAR,
    TYPE_SIGNIN
  };

  // Gets EasyUnlockService instance.
  static EasyUnlockService* Get(Profile* profile);

  // Gets EasyUnlockService instance associated with a user if the user is
  // logged in and his profile is initialized.
  static EasyUnlockService* GetForUser(const user_manager::User& user);

  // Registers Easy Unlock profile preferences.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Registers Easy Unlock local state entries.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Removes the hardlock state for the given user.
  static void RemoveHardlockStateForUser(const std::string& user_id);

  // Returns the EasyUnlockService type.
  virtual Type GetType() const = 0;

  // Returns the user currently associated with the service.
  virtual std::string GetUserEmail() const = 0;

  // Launches Easy Unlock Setup app.
  virtual void LaunchSetup() = 0;

  // Gets/Sets/Clears the permit access for the local device.
  virtual const base::DictionaryValue* GetPermitAccess() const = 0;
  virtual void SetPermitAccess(const base::DictionaryValue& permit) = 0;
  virtual void ClearPermitAccess() = 0;

  // Gets/Sets/Clears the remote devices list.
  virtual const base::ListValue* GetRemoteDevices() const = 0;
  virtual void SetRemoteDevices(const base::ListValue& devices) = 0;
  virtual void ClearRemoteDevices() = 0;

  // Runs the flow for turning Easy unlock off.
  virtual void RunTurnOffFlow() = 0;

  // Resets the turn off flow if one is in progress.
  virtual void ResetTurnOffFlow() = 0;

  // Returns the current turn off flow status.
  virtual TurnOffFlowStatus GetTurnOffFlowStatus() const = 0;

  // Gets the challenge bytes for the user currently associated with the
  // service.
  virtual std::string GetChallenge() const = 0;

  // Retrieved wrapped secret that should be used to unlock cryptohome for the
  // user currently associated with the service. If the service does not support
  // signin (i.e. service for a regular profile) or there is no secret available
  // for the user, returns an empty string.
  virtual std::string GetWrappedSecret() const = 0;

  // Whether easy unlock is allowed to be used. If the controlling preference
  // is set (from policy), this returns the preference value. Otherwise, it is
  // permitted either the flag is enabled or its field trial is enabled.
  bool IsAllowed();

  // Sets the hardlock state for the associated user.
  void SetHardlockState(EasyUnlockScreenlockStateHandler::HardlockState state);

  // Returns the hardlock state for the associated user.
  EasyUnlockScreenlockStateHandler::HardlockState GetHardlockState() const;

  // Ensures the hardlock state is visible even when there is no cryptohome
  // keys and no state update from the app.
  void MaybeShowHardlockUI();

  // Updates the user pod on the signin/lock screen for the user associated with
  // the service to reflect the provided screenlock state.
  bool UpdateScreenlockState(EasyUnlockScreenlockStateHandler::State state);

  // Starts an auth attempt for the user associated with the service. The
  // attempt type (unlock vs. signin) will depend on the service type.
  void AttemptAuth(const std::string& user_id);

  // Finalizes the previously started auth attempt for easy unlock. If called on
  // signin profile service, it will cancel the current auth attempt if one
  // exists.
  void FinalizeUnlock(bool success);

  // Finalizes previously started auth attempt for easy signin. If called on
  // regular profile service, it will cancel the current auth attempt if one
  // exists.
  void FinalizeSignin(const std::string& secret);

  // Checks the consistency between pairing data and cryptohome keys. Set
  // hardlock state if the two do not match.
  void CheckCryptohomeKeysAndMaybeHardlock();

  void AddObserver(EasyUnlockServiceObserver* observer);
  void RemoveObserver(EasyUnlockServiceObserver* observer);

 protected:
  explicit EasyUnlockService(Profile* profile);
  virtual ~EasyUnlockService();

  // Does a service type specific initialization.
  virtual void InitializeInternal() = 0;

  // Does a service type specific shutdown. Called from |Shutdown|.
  virtual void ShutdownInternal() = 0;

  // Service type specific tests for whether the service is allowed. Returns
  // false if service is not allowed. If true is returned, the service may still
  // not be allowed if common tests fail (e.g. if Bluetooth is not available).
  virtual bool IsAllowedInternal() = 0;

  // KeyedService override:
  virtual void Shutdown() OVERRIDE;

  // Exposes the profile to which the service is attached to subclasses.
  Profile* profile() const { return profile_; }

  // Installs the Easy unlock component app if it isn't installed and enables
  // the app if it is disabled.
  void LoadApp();

  // Disables the Easy unlock component app if it's loaded.
  void DisableAppIfLoaded();

  // Unloads the Easy unlock component app if it's loaded.
  void UnloadApp();

  // Reloads the Easy unlock component app if it's loaded.
  void ReloadApp();

  // Checks whether Easy unlock should be running and updates app state.
  void UpdateAppState();

  // Notifies the easy unlock app that the user state has been updated.
  void NotifyUserUpdated();

  // Notifies observers that the turn off flow status changed.
  void NotifyTurnOffOperationStatusChanged();

  // Resets the screenlock state set by this service.
  void ResetScreenlockState();

  // Updates |screenlock_state_handler_|'s hardlocked state.
  void SetScreenlockHardlockedState(
      EasyUnlockScreenlockStateHandler::HardlockState state);

 private:
  // A class to detect whether a bluetooth adapter is present.
  class BluetoothDetector;

  // Initializes the service after ExtensionService is ready.
  void Initialize();

  // Gets |screenlock_state_handler_|. Returns NULL if Easy Unlock is not
  // allowed. Otherwise, if |screenlock_state_handler_| is not set, an instance
  // is created. Do not cache the returned value, as it may go away if Easy
  // Unlock gets disabled.
  EasyUnlockScreenlockStateHandler* GetScreenlockStateHandler();

  // Callback when Bluetooth adapter present state changes.
  void OnBluetoothAdapterPresentChanged();

  // Saves hardlock state for the given user. Update UI if the currently
  // associated user is the same.
  void SetHardlockStateForUser(
      const std::string& user_id,
      EasyUnlockScreenlockStateHandler::HardlockState state);

#if defined(OS_CHROMEOS)
  // Callback for get key operation from CheckCryptohomeKeysAndMaybeHardlock.
  void OnCryptohomeKeysFetchedForChecking(
      const std::string& user_id,
      const std::set<std::string> paired_devices,
      bool success,
      const chromeos::EasyUnlockDeviceKeyDataList& key_data_list);
#endif

  Profile* profile_;

  // Created lazily in |GetScreenlockStateHandler|.
  scoped_ptr<EasyUnlockScreenlockStateHandler> screenlock_state_handler_;

  // The handler for the current auth attempt. Set iff an auth attempt is in
  // progress.
  scoped_ptr<EasyUnlockAuthAttempt> auth_attempt_;

  scoped_ptr<BluetoothDetector> bluetooth_detector_;

#if defined(OS_CHROMEOS)
  // Monitors suspend and wake state of ChromeOS.
  class PowerMonitor;
  scoped_ptr<PowerMonitor> power_monitor_;
#endif

  // Whether the service has been shut down.
  bool shut_down_;

  ObserverList<EasyUnlockServiceObserver> observers_;

  base::WeakPtrFactory<EasyUnlockService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockService);
};

#endif  // CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_H_
