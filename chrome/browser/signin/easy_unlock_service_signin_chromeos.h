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
  virtual ~EasyUnlockServiceSignin();

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
  virtual EasyUnlockService::Type GetType() const OVERRIDE;
  virtual std::string GetUserEmail() const OVERRIDE;
  virtual void LaunchSetup() OVERRIDE;
  virtual const base::DictionaryValue* GetPermitAccess() const OVERRIDE;
  virtual void SetPermitAccess(const base::DictionaryValue& permit) OVERRIDE;
  virtual void ClearPermitAccess() OVERRIDE;
  virtual const base::ListValue* GetRemoteDevices() const OVERRIDE;
  virtual void SetRemoteDevices(const base::ListValue& devices) OVERRIDE;
  virtual void ClearRemoteDevices() OVERRIDE;
  virtual void RunTurnOffFlow() OVERRIDE;
  virtual void ResetTurnOffFlow() OVERRIDE;
  virtual TurnOffFlowStatus GetTurnOffFlowStatus() const OVERRIDE;
  virtual std::string GetChallenge() const OVERRIDE;
  virtual std::string GetWrappedSecret() const OVERRIDE;
  virtual void InitializeInternal() OVERRIDE;
  virtual void ShutdownInternal() OVERRIDE;
  virtual bool IsAllowedInternal() OVERRIDE;

  // ScreenlockBridge::Observer implementation:
  virtual void OnScreenDidLock() OVERRIDE;
  virtual void OnScreenDidUnlock() OVERRIDE;
  virtual void OnFocusedUserChanged(const std::string& user_id) OVERRIDE;

  // chromeos::LoginState::Observer implementation:
  virtual void LoggedInStateChanged() OVERRIDE;

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
