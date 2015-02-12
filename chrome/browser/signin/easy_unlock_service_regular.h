// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_REGULAR_H_
#define CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_REGULAR_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/signin/easy_unlock_service.h"
#include "chrome/browser/signin/screenlock_bridge.h"

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
}

class EasyUnlockAppManager;
class EasyUnlockToggleFlow;
class Profile;

// EasyUnlockService instance that should be used for regular, non-signin
// profiles.
class EasyUnlockServiceRegular : public EasyUnlockService,
                                 public ScreenlockBridge::Observer {
 public:
  explicit EasyUnlockServiceRegular(Profile* profile);
  ~EasyUnlockServiceRegular() override;

 private:
  // EasyUnlockService implementation:
  EasyUnlockService::Type GetType() const override;
  std::string GetUserEmail() const override;
  void LaunchSetup() override;
  const base::DictionaryValue* GetPermitAccess() const override;
  void SetPermitAccess(const base::DictionaryValue& permit) override;
  void ClearPermitAccess() override;
  const base::ListValue* GetRemoteDevices() const override;
  void SetRemoteDevices(const base::ListValue& devices) override;
  void RunTurnOffFlow() override;
  void ResetTurnOffFlow() override;
  TurnOffFlowStatus GetTurnOffFlowStatus() const override;
  std::string GetChallenge() const override;
  std::string GetWrappedSecret() const override;
  void RecordEasySignInOutcome(const std::string& user_id,
                               bool success) const override;
  void RecordPasswordLoginEvent(const std::string& user_id) const override;
  void StartAutoPairing(const AutoPairingResultCallback& callback) override;
  void SetAutoPairingResult(bool success, const std::string& error) override;
  void InitializeInternal() override;
  void ShutdownInternal() override;
  bool IsAllowedInternal() const override;
  void OnWillFinalizeUnlock(bool success) override;

  // ScreenlockBridge::Observer implementation:
  void OnScreenDidLock(
      ScreenlockBridge::LockHandler::ScreenType screen_type) override;
  void OnScreenDidUnlock(
      ScreenlockBridge::LockHandler::ScreenType screen_type) override;
  void OnFocusedUserChanged(const std::string& user_id) override;

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

  scoped_ptr<chromeos::ShortLivedUserContext> short_lived_user_context_;
#endif

  // Updates local state with the preference from the user's profile, so they
  // can be accessed on the sign-in screen.
  void SyncProfilePrefsToLocalState();

  PrefChangeRegistrar registrar_;

  TurnOffFlowStatus turn_off_flow_status_;
  scoped_ptr<proximity_auth::CryptAuthClient> cryptauth_client_;

  AutoPairingResultCallback auto_pairing_callback_;

  // True if the user just unlocked the screen using Easy Unlock. Reset once
  // the screen unlocks. Used to distinguish Easy Unlock-powered unlocks from
  // password-based unlocks for metrics.
  bool will_unlock_using_easy_unlock_;

  base::WeakPtrFactory<EasyUnlockServiceRegular> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockServiceRegular);
};

#endif  // CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_REGULAR_H_
