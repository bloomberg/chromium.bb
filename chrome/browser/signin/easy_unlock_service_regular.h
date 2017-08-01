// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_REGULAR_H_
#define CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_REGULAR_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/signin/easy_unlock_service.h"
#include "components/cryptauth/cryptauth_device_manager.h"
#include "components/proximity_auth/screenlock_bridge.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/easy_unlock/short_lived_user_context.h"
#endif

namespace base {
class DictionaryValue;
class ListValue;
}

namespace cryptauth {
class CryptAuthClient;
class CryptAuthDeviceManager;
class CryptAuthEnrollmentManager;
class LocalDeviceDataProvider;
class RemoteDeviceLoader;
class ToggleEasyUnlockResponse;
}

namespace proximity_auth {
class PromotionManager;
class ProximityAuthProfilePrefManager;
}

class EasyUnlockNotificationController;
class Profile;

// EasyUnlockService instance that should be used for regular, non-signin
// profiles.
class EasyUnlockServiceRegular
    : public EasyUnlockService,
      public proximity_auth::ScreenlockBridge::Observer,
      public cryptauth::CryptAuthDeviceManager::Observer {
 public:
  explicit EasyUnlockServiceRegular(Profile* profile);

  // Constructor for tests.
  EasyUnlockServiceRegular(Profile* profile,
                           std::unique_ptr<EasyUnlockNotificationController>
                               notification_controller);

  ~EasyUnlockServiceRegular() override;

 private:
  // Loads the RemoteDevice instances that will be supplied to
  // ProximityAuthSystem.
  void LoadRemoteDevices();

  // Called when |remote_device_loader_| completes.
  void OnRemoteDevicesLoaded(
      const std::vector<cryptauth::RemoteDevice>& remote_devices);

  // True if we should promote EasyUnlock.
  bool ShouldPromote();

  // Starts the promotion manager to periodically display an EasyUnlock
  // promotion.
  void StartPromotionManager();

  // EasyUnlockService implementation:
  proximity_auth::ProximityAuthPrefManager* GetProximityAuthPrefManager()
      override;
  EasyUnlockService::Type GetType() const override;
  AccountId GetAccountId() const override;
  void LaunchSetup() override;
  const base::DictionaryValue* GetPermitAccess() const override;
  void SetPermitAccess(const base::DictionaryValue& permit) override;
  void ClearPermitAccess() override;
  const base::ListValue* GetRemoteDevices() const override;
  void SetRemoteDevices(const base::ListValue& devices) override;
  void SetRemoteBleDevices(const base::ListValue& devices) override;
  void RunTurnOffFlow() override;
  void ResetTurnOffFlow() override;
  TurnOffFlowStatus GetTurnOffFlowStatus() const override;
  std::string GetChallenge() const override;
  std::string GetWrappedSecret() const override;
  void RecordEasySignInOutcome(const AccountId& account_id,
                               bool success) const override;
  void RecordPasswordLoginEvent(const AccountId& account_id) const override;
  void StartAutoPairing(const AutoPairingResultCallback& callback) override;
  void SetAutoPairingResult(bool success, const std::string& error) override;
  void InitializeInternal() override;
  void ShutdownInternal() override;
  bool IsAllowedInternal() const override;
  void OnWillFinalizeUnlock(bool success) override;
  void OnSuspendDoneInternal() override;
#if defined(OS_CHROMEOS)
  void HandleUserReauth(const chromeos::UserContext& user_context) override;
#endif

  // CryptAuthDeviceManager::Observer:
  void OnSyncStarted() override;
  void OnSyncFinished(
      cryptauth::CryptAuthDeviceManager::SyncResult sync_result,
      cryptauth::CryptAuthDeviceManager::DeviceChangeResult
          device_change_result) override;

  // proximity_auth::ScreenlockBridge::Observer implementation:
  void OnScreenDidLock(proximity_auth::ScreenlockBridge::LockHandler::ScreenType
                           screen_type) override;
  void OnScreenDidUnlock(
      proximity_auth::ScreenlockBridge::LockHandler::ScreenType screen_type)
      override;
  void OnFocusedUserChanged(const AccountId& account_id) override;

  // Sets the new turn-off flow status.
  void SetTurnOffFlowStatus(TurnOffFlowStatus status);

  // Callback for ToggleEasyUnlock CryptAuth API.
  void OnToggleEasyUnlockApiComplete(
      const cryptauth::ToggleEasyUnlockResponse& response);
  void OnToggleEasyUnlockApiFailed(const std::string& error_message);

#if defined(OS_CHROMEOS)
  // Called with the user's credentials (e.g. username and password) after the
  // user reauthenticates to begin setup.
  void OpenSetupAppAfterReauth(const chromeos::UserContext& user_context);

  // Called after a cryptohome RemoveKey or RefreshKey operation to set the
  // proper hardlock state if the operation is successful.
  void SetHardlockAfterKeyOperation(
      EasyUnlockScreenlockStateHandler::HardlockState state_on_success,
      bool success);

  std::unique_ptr<chromeos::ShortLivedUserContext> short_lived_user_context_;
#endif

  // Updates local state with the preference from the user's profile, so they
  // can be accessed on the sign-in screen.
  void SyncProfilePrefsToLocalState();

  // Returns the CryptAuthEnrollmentManager, which manages the profile's
  // CryptAuth enrollment.
  cryptauth::CryptAuthEnrollmentManager* GetCryptAuthEnrollmentManager();

  // Returns the CryptAuthEnrollmentManager, which manages the profile's
  // synced devices from CryptAuth.
  cryptauth::CryptAuthDeviceManager* GetCryptAuthDeviceManager();

  TurnOffFlowStatus turn_off_flow_status_;
  std::unique_ptr<cryptauth::CryptAuthClient> cryptauth_client_;

  AutoPairingResultCallback auto_pairing_callback_;

  // True if the user just unlocked the screen using Easy Unlock. Reset once
  // the screen unlocks. Used to distinguish Easy Unlock-powered unlocks from
  // password-based unlocks for metrics.
  bool will_unlock_using_easy_unlock_;

  // The timestamp for the most recent time when the lock screen was shown. The
  // lock screen is typically shown when the user awakens their computer from
  // sleep -- e.g. by opening the lid -- but can also be shown if the screen is
  // locked but the computer does not go to sleep.
  base::TimeTicks lock_screen_last_shown_timestamp_;

  // Manager responsible for handling the prefs used by proximity_auth classes.
  std::unique_ptr<proximity_auth::ProximityAuthProfilePrefManager>
      pref_manager_;

  // Loads the RemoteDevice instances from CryptAuth and local data.
  std::unique_ptr<cryptauth::RemoteDeviceLoader> remote_device_loader_;

  // Provides local device information from CryptAuth.
  std::unique_ptr<cryptauth::LocalDeviceDataProvider>
      local_device_data_provider_;

  // Manager responsible for display EasyUnlock promotions to the user.
  std::unique_ptr<proximity_auth::PromotionManager> promotion_manager_;

  // If a new RemoteDevice was synced while the screen is locked, we defer
  // loading the RemoteDevice until the screen is unlocked. For security,
  // this deferment prevents the lock screen from being changed by a network
  // event.
  bool deferring_device_load_;

  // Responsible for showing all the notifications used for EasyUnlock.
  std::unique_ptr<EasyUnlockNotificationController> notification_controller_;

  // Stores the unlock keys for EasyUnlock before the current device sync, so we
  // can compare it to the unlock keys after syncing.
  std::vector<cryptauth::ExternalDeviceInfo> unlock_keys_before_sync_;

  // True if the pairing changed notification was shown, so that the next time
  // the Chromebook is unlocked, we can show the subsequent 'pairing applied'
  // notification.
  bool shown_pairing_changed_notification_;

  base::WeakPtrFactory<EasyUnlockServiceRegular> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockServiceRegular);
};

#endif  // CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_REGULAR_H_
