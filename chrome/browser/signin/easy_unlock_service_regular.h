// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_REGULAR_H_
#define CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_REGULAR_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/signin/easy_unlock_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/proximity_auth/cryptauth/cryptauth_device_manager.h"
#include "components/proximity_auth/screenlock_bridge.h"
#include "google_apis/gaia/oauth2_token_service.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/easy_unlock/short_lived_user_context.h"
#endif

namespace base {
class DictionaryValue;
class ListValue;
}

namespace cryptauth {
class ToggleEasyUnlockResponse;
}

namespace proximity_auth {
class CryptAuthClient;
class CryptAuthDeviceManager;
class CryptAuthEnrollmentManager;
class CryptAuthGCMManager;
class ProximityAuthPrefManager;
class RemoteDeviceLoader;
}

class EasyUnlockAppManager;
class EasyUnlockToggleFlow;
class Profile;

// EasyUnlockService instance that should be used for regular, non-signin
// profiles.
class EasyUnlockServiceRegular
    : public EasyUnlockService,
      public proximity_auth::ScreenlockBridge::Observer,
      public proximity_auth::CryptAuthDeviceManager::Observer,
      public OAuth2TokenService::Observer {
 public:
  explicit EasyUnlockServiceRegular(Profile* profile);
  ~EasyUnlockServiceRegular() override;

  // Returns the CryptAuthEnrollmentManager, which manages the profile's
  // CryptAuth enrollment.
  proximity_auth::CryptAuthEnrollmentManager* GetCryptAuthEnrollmentManager();

  // Returns the CryptAuthEnrollmentManager, which manages the profile's
  // synced devices from CryptAuth.
  proximity_auth::CryptAuthDeviceManager* GetCryptAuthDeviceManager();

  // Returns the ProximityAuthPrefManager, which manages the profile's
  // prefs for proximity_auth classes.
  proximity_auth::ProximityAuthPrefManager* GetProximityAuthPrefManager();

 private:
  // Loads the RemoteDevice instances that will be supplied to
  // ProximityAuthSystem.
  void LoadRemoteDevices();

  // Called when |remote_device_loader_| completes.
  void OnRemoteDevicesLoaded(
      const std::vector<proximity_auth::RemoteDevice>& remote_devices);

  // EasyUnlockService implementation:
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

  // OAuth2TokenService::Observer:
  void OnRefreshTokenAvailable(const std::string& account_id) override;

  // CryptAuthDeviceManager::Observer:
  void OnSyncFinished(
      proximity_auth::CryptAuthDeviceManager::SyncResult sync_result,
      proximity_auth::CryptAuthDeviceManager::DeviceChangeResult
          device_change_result) override;

  // proximity_auth::ScreenlockBridge::Observer implementation:
  void OnScreenDidLock(proximity_auth::ScreenlockBridge::LockHandler::ScreenType
                           screen_type) override;
  void OnScreenDidUnlock(
      proximity_auth::ScreenlockBridge::LockHandler::ScreenType screen_type)
      override;
  void OnFocusedUserChanged(const AccountId& account_id) override;

  // Callback when the controlling pref changes.
  void OnPrefsChanged();

  // Sets the new turn-off flow status.
  void SetTurnOffFlowStatus(TurnOffFlowStatus status);

  // Callback for ToggleEasyUnlock CryptAuth API.
  void OnToggleEasyUnlockApiComplete(
      const cryptauth::ToggleEasyUnlockResponse& response);
  void OnToggleEasyUnlockApiFailed(const std::string& error_message);

#if defined(OS_CHROMEOS)
  // Called with the user's credentials (e.g. username and password) after the
  // user reauthenticates to begin setup.
  void OnUserContextFromReauth(const chromeos::UserContext& user_context);

  // Called after a cryptohome RemoveKey or RefreshKey operation to set the
  // proper hardlock state if the operation is successful.
  void SetHardlockAfterKeyOperation(
      EasyUnlockScreenlockStateHandler::HardlockState state_on_success,
      bool success);

  // Initializes the managers that communicate with CryptAuth.
  void InitializeCryptAuth();

  scoped_ptr<chromeos::ShortLivedUserContext> short_lived_user_context_;
#endif

  // Updates local state with the preference from the user's profile, so they
  // can be accessed on the sign-in screen.
  void SyncProfilePrefsToLocalState();

  // Returns the base GcmDeviceInfo proto containing the device's platform and
  // version information.
  cryptauth::GcmDeviceInfo GetGcmDeviceInfo();

  PrefChangeRegistrar registrar_;

  TurnOffFlowStatus turn_off_flow_status_;
  scoped_ptr<proximity_auth::CryptAuthClient> cryptauth_client_;

  AutoPairingResultCallback auto_pairing_callback_;

  // True if the user just unlocked the screen using Easy Unlock. Reset once
  // the screen unlocks. Used to distinguish Easy Unlock-powered unlocks from
  // password-based unlocks for metrics.
  bool will_unlock_using_easy_unlock_;

  // The timestamp for the most recent time when the lock screen was shown. The
  // lock screen is typically shown when the user awakens her computer from
  // sleep -- e.g. by opening the lid -- but can also be shown if the screen is
  // locked but the computer does not go to sleep.
  base::TimeTicks lock_screen_last_shown_timestamp_;

  // Managers responsible for handling syncing and communications with
  // CryptAuth.
  scoped_ptr<proximity_auth::CryptAuthGCMManager> gcm_manager_;
  scoped_ptr<proximity_auth::CryptAuthEnrollmentManager> enrollment_manager_;
  scoped_ptr<proximity_auth::CryptAuthDeviceManager> device_manager_;

  // Manager responsible for handling the prefs used by proximity_auth classes.
  scoped_ptr<proximity_auth::ProximityAuthPrefManager> pref_manager_;

  // Loads the RemoteDevice instances from CryptAuth and local data.
  scoped_ptr<proximity_auth::RemoteDeviceLoader> remote_device_loader_;

  // If a new RemoteDevice was synced while the screen is locked, we defer
  // loading the RemoteDevice until the screen is unlocked. For security,
  // this deferment prevents the lock screen from being changed by a network
  // event.
  bool deferring_device_load_;

  base::WeakPtrFactory<EasyUnlockServiceRegular> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockServiceRegular);
};

#endif  // CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_REGULAR_H_
