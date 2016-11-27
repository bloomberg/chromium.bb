// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_H_

#include <memory>
#include <set>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "chrome/browser/signin/chrome_proximity_auth_client.h"
#include "chrome/browser/signin/easy_unlock_auth_attempt.h"
#include "chrome/browser/signin/easy_unlock_metrics.h"
#include "chrome/browser/signin/easy_unlock_screenlock_state_handler.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/proximity_auth/remote_device.h"
#include "components/proximity_auth/screenlock_state.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_types.h"
#endif

class AccountId;

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

namespace proximity_auth {
class ProximityAuthSystem;
}

class EasyUnlockAppManager;
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

  // Easy Unlock settings that the user can configure.
  struct UserSettings {
    UserSettings();
    ~UserSettings();

    // Whether to require the remote device to be in very close proximity
    // before allowing unlock (~1 feet).
    bool require_close_proximity;
  };

  // Gets EasyUnlockService instance.
  static EasyUnlockService* Get(Profile* profile);

  // Gets EasyUnlockService instance associated with a user if the user is
  // logged in and their profile is initialized.
  static EasyUnlockService* GetForUser(const user_manager::User& user);

  // Registers Easy Unlock profile preferences.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Registers Easy Unlock local state entries.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Removes the hardlock state for the given user.
  static void ResetLocalStateForUser(const AccountId& account_id);

  // Returns the user's preferences.
  static UserSettings GetUserSettings(const AccountId& account_id);

  // Returns the identifier for the device.
  static std::string GetDeviceId();

  // Returns the EasyUnlockService type.
  virtual Type GetType() const = 0;

  // Returns the user currently associated with the service.
  virtual AccountId GetAccountId() const = 0;

  // Launches Easy Unlock setup app.
  virtual void LaunchSetup() = 0;

  // Gets/Sets/Clears the permit access for the local device.
  virtual const base::DictionaryValue* GetPermitAccess() const = 0;
  virtual void SetPermitAccess(const base::DictionaryValue& permit) = 0;
  virtual void ClearPermitAccess() = 0;

  // Gets/Sets the remote devices list.
  virtual const base::ListValue* GetRemoteDevices() const = 0;
  virtual void SetRemoteDevices(const base::ListValue& devices) = 0;
  virtual void SetRemoteBleDevices(const base::ListValue& devices) = 0;

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

  // Records metrics for Easy sign-in outcome for the given user.
  virtual void RecordEasySignInOutcome(const AccountId& account_id,
                                       bool success) const = 0;

  // Records metrics for password based flow for the given user.
  virtual void RecordPasswordLoginEvent(const AccountId& account_id) const = 0;

  // Starts auto pairing.
  typedef base::Callback<void(bool success, const std::string& error)>
      AutoPairingResultCallback;
  virtual void StartAutoPairing(const AutoPairingResultCallback& callback) = 0;

  // Sets auto pairing result.
  virtual void SetAutoPairingResult(bool success, const std::string& error) = 0;

  // Sets the service up and schedules service initialization.
  void Initialize(std::unique_ptr<EasyUnlockAppManager> app_manager);

  // Whether easy unlock is allowed to be used. If the controlling preference
  // is set (from policy), this returns the preference value. Otherwise, it is
  // permitted if the flag is enabled. Virtual to allow override for testing.
  virtual bool IsAllowed() const;

  // Whether Easy Unlock is currently enabled for this user. Virtual to allow
  // override for testing.
  virtual bool IsEnabled() const;

  // Sets the hardlock state for the associated user.
  void SetHardlockState(EasyUnlockScreenlockStateHandler::HardlockState state);

  // Returns the hardlock state for the associated user.
  EasyUnlockScreenlockStateHandler::HardlockState GetHardlockState() const;

  // Gets the persisted hardlock state. Return true if there is persisted
  // hardlock state and the value would be set to |state|. Otherwise,
  // returns false and |state| is unchanged.
  bool GetPersistedHardlockState(
      EasyUnlockScreenlockStateHandler::HardlockState* state) const;

  // Shows the hardlock or connecting state as initial UI before cryptohome
  // keys checking and state update from the app.
  void ShowInitialUserState();

  // Updates the user pod on the signin/lock screen for the user associated with
  // the service to reflect the provided screenlock state.
  bool UpdateScreenlockState(proximity_auth::ScreenlockState state);

  // Returns the screenlock state if it is available. Otherwise STATE_INACTIVE
  // is returned.
  proximity_auth::ScreenlockState GetScreenlockState();

  // Starts an auth attempt for the user associated with the service. The
  // attempt type (unlock vs. signin) will depend on the service type.
  void AttemptAuth(const AccountId& account_id);

  // Similar to above but a callback is invoked after the auth attempt is
  // finalized instead of default unlock/sign-in.
  typedef EasyUnlockAuthAttempt::FinalizedCallback AttemptAuthCallback;
  void AttemptAuth(const AccountId& account_id,
                   const AttemptAuthCallback& callback);

  // Finalizes the previously started auth attempt for easy unlock. If called on
  // signin profile service, it will cancel the current auth attempt if one
  // exists.
  void FinalizeUnlock(bool success);

  // Finalizes previously started auth attempt for easy signin. If called on
  // regular profile service, it will cancel the current auth attempt if one
  // exists.
  void FinalizeSignin(const std::string& secret);

  // Handles Easy Unlock auth failure for the user.
  void HandleAuthFailure(const AccountId& account_id);

  // Checks the consistency between pairing data and cryptohome keys. Set
  // hardlock state if the two do not match.
  void CheckCryptohomeKeysAndMaybeHardlock();

  // Marks the Easy Unlock screen lock state as the one associated with the
  // trial run initiated by Easy Unlock app.
  void SetTrialRun();

  // Records that the user clicked on the lock icon during the trial run
  // initiated by the Easy Unlock app.
  void RecordClickOnLockIcon();

  void AddObserver(EasyUnlockServiceObserver* observer);
  void RemoveObserver(EasyUnlockServiceObserver* observer);

  ChromeProximityAuthClient* proximity_auth_client() {
    return &proximity_auth_client_;
  }

 protected:
  explicit EasyUnlockService(Profile* profile);
  ~EasyUnlockService() override;

  // Does a service type specific initialization.
  virtual void InitializeInternal() = 0;

  // Does a service type specific shutdown. Called from |Shutdown|.
  virtual void ShutdownInternal() = 0;

  // Service type specific tests for whether the service is allowed. Returns
  // false if service is not allowed. If true is returned, the service may still
  // not be allowed if common tests fail (e.g. if Bluetooth is not available).
  virtual bool IsAllowedInternal() const = 0;

  // Called while processing a user gesture to unlock the screen using Easy
  // Unlock, just before the screen is unlocked.
  virtual void OnWillFinalizeUnlock(bool success) = 0;

  // Called when the local device resumes after a suspend.
  virtual void OnSuspendDoneInternal() = 0;

  // KeyedService override:
  void Shutdown() override;

  // Exposes the profile to which the service is attached to subclasses.
  const Profile* profile() const { return profile_; }
  Profile* profile() { return profile_; }

  // Opens an Easy Unlock Setup app window.
  void OpenSetupApp();

  // Reloads the Easy unlock component app if it's loaded and resets the lock
  // screen state.
  void ReloadAppAndLockScreen();

  // Checks whether Easy unlock should be running and updates app state.
  void UpdateAppState();

  // Disables easy unlock app without affecting lock screen state.
  // Used primarily by signin service when user logged in state changes to
  // logged in but before screen gets unlocked. At this point service shutdown
  // is imminent and the app can be safely unloaded, but, for esthetic reasons,
  // the lock screen UI should remain unchanged until the screen unlocks.
  void DisableAppWithoutResettingScreenlockState();

  // Notifies the easy unlock app that the user state has been updated.
  void NotifyUserUpdated();

  // Notifies observers that the turn off flow status changed.
  void NotifyTurnOffOperationStatusChanged();

  // Resets the screenlock state set by this service.
  void ResetScreenlockState();

  // Updates |screenlock_state_handler_|'s hardlocked state.
  void SetScreenlockHardlockedState(
      EasyUnlockScreenlockStateHandler::HardlockState state);

  const EasyUnlockScreenlockStateHandler* screenlock_state_handler() const {
    return screenlock_state_handler_.get();
  }

  // Saves hardlock state for the given user. Update UI if the currently
  // associated user is the same.
  void SetHardlockStateForUser(
      const AccountId& account_id,
      EasyUnlockScreenlockStateHandler::HardlockState state);

  // Returns the authentication event for a recent password sign-in or unlock,
  // according to the current state of the service.
  EasyUnlockAuthEvent GetPasswordAuthEvent() const;

  // Called by subclasses when remote devices allowed to unlock the screen
  // are loaded for |account_id|.
  void SetProximityAuthDevices(
      const AccountId& account_id,
      const proximity_auth::RemoteDeviceList& remote_devices);

 private:
  // A class to detect whether a bluetooth adapter is present.
  class BluetoothDetector;

  // Initializes the service after EasyUnlockAppManager is ready.
  void InitializeOnAppManagerReady();

  // Gets |screenlock_state_handler_|. Returns NULL if Easy Unlock is not
  // allowed. Otherwise, if |screenlock_state_handler_| is not set, an instance
  // is created. Do not cache the returned value, as it may go away if Easy
  // Unlock gets disabled.
  EasyUnlockScreenlockStateHandler* GetScreenlockStateHandler();

  // Callback when Bluetooth adapter present state changes.
  void OnBluetoothAdapterPresentChanged();

#if defined(OS_CHROMEOS)
  // Callback for get key operation from CheckCryptohomeKeysAndMaybeHardlock.
  void OnCryptohomeKeysFetchedForChecking(
      const AccountId& account_id,
      const std::set<std::string> paired_devices,
      bool success,
      const chromeos::EasyUnlockDeviceKeyDataList& key_data_list);
#endif

  // Updates the service to state for handling system suspend.
  void PrepareForSuspend();

  // Called when the system resumes from a suspended state.
  void OnSuspendDone();

  void EnsureTpmKeyPresentIfNeeded();

  Profile* const profile_;

  ChromeProximityAuthClient proximity_auth_client_;

  std::unique_ptr<EasyUnlockAppManager> app_manager_;

  // Created lazily in |GetScreenlockStateHandler|.
  std::unique_ptr<EasyUnlockScreenlockStateHandler> screenlock_state_handler_;

  // The handler for the current auth attempt. Set iff an auth attempt is in
  // progress.
  std::unique_ptr<EasyUnlockAuthAttempt> auth_attempt_;

  // Detects when the system Bluetooth adapter status changes.
  std::unique_ptr<BluetoothDetector> bluetooth_detector_;

  // Handles connecting, authenticating, and updating the UI on the lock/sign-in
  // screen. After a |RemoteDevice| instance is provided, this object will
  // handle the rest.
  // TODO(tengs): This object is intended as a replacement of the background
  // page of the easy_unlock Chrome app. We are in the process of removing the
  // app in favor of |proximity_auth_system_|.
  std::unique_ptr<proximity_auth::ProximityAuthSystem> proximity_auth_system_;

#if defined(OS_CHROMEOS)
  // Monitors suspend and wake state of ChromeOS.
  class PowerMonitor;
  std::unique_ptr<PowerMonitor> power_monitor_;
#endif

  // Whether the service has been shut down.
  bool shut_down_;

  bool tpm_key_checked_;

  base::ObserverList<EasyUnlockServiceObserver> observers_;

  base::WeakPtrFactory<EasyUnlockService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockService);
};

#endif  // CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_H_
