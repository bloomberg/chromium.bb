// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_SIGNIN_CHROMEOS_H_
#define CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_SIGNIN_CHROMEOS_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_types.h"
#include "chrome/browser/signin/easy_unlock_service.h"
#include "chrome/browser/signin/screenlock_bridge.h"
#include "chromeos/login/login_state.h"

// EasyUnlockService instance that should be used for signin profile.
class EasyUnlockServiceSignin : public EasyUnlockService,
                                public ScreenlockBridge::Observer,
                                public chromeos::LoginState::Observer {
 public:
  explicit EasyUnlockServiceSignin(Profile* profile);
  ~EasyUnlockServiceSignin() override;

 private:
  // The load state of a user's cryptohome key data.
  enum UserDataState {
    // Initial state, the key data is empty and not being loaded.
    USER_DATA_STATE_INITIAL,
    // The key data is empty, but being loaded.
    USER_DATA_STATE_LOADING,
    // The key data has been loaded.
    USER_DATA_STATE_LOADED
  };

  // Structure containing a user's key data loaded from cryptohome.
  struct UserData {
    UserData();
    ~UserData();

    // The loading state of the data.
    UserDataState state;

    // The data as returned from cryptohome.
    chromeos::EasyUnlockDeviceKeyDataList devices;

    // The list of remote device dictionaries understood by Easy unlock app.
    // This will be returned by |GetRemoteDevices| method.
    base::ListValue remote_devices_value;

   private:
    DISALLOW_COPY_AND_ASSIGN(UserData);
  };

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

  // chromeos::LoginState::Observer implementation:
  void LoggedInStateChanged() override;

  // Loads the device data associated with the user's Easy unlock keys from
  // crypthome.
  void LoadCurrentUserDataIfNeeded();

  // Callback invoked when the user's device data is loaded from cryptohome.
  void OnUserDataLoaded(
      const std::string& user_id,
      bool success,
      const chromeos::EasyUnlockDeviceKeyDataList& data);

  // If the device data has been loaded for the current user, returns it.
  // Otherwise, returns NULL.
  const UserData* FindLoadedDataForCurrentUser() const;

  // User id of the user currently associated with the service.
  std::string user_id_;

  // Maps user ids to their fetched cryptohome key data.
  std::map<std::string, UserData*> user_data_;

  // Whether failed attempts to load user data should be retried.
  // This is to handle case where cryptohome daemon is not started in time the
  // service attempts to load some data. Retries will be allowed only until the
  // first data load finishes (even if it fails).
  bool allow_cryptohome_backoff_;

  // Whether the service has been successfully initialized, and has not been
  // shut down.
  bool service_active_;

  base::WeakPtrFactory<EasyUnlockServiceSignin> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockServiceSignin);
};

#endif  // CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_SIGNIN_CHROMEOS_H_
