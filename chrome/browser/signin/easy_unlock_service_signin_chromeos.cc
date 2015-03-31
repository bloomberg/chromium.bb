// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/easy_unlock_service_signin_chromeos.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/sys_info.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_key_manager.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/signin/easy_unlock_app_manager.h"
#include "chrome/browser/signin/easy_unlock_metrics.h"
#include "chromeos/login/auth/user_context.h"
#include "chromeos/tpm/tpm_token_loader.h"

namespace {

// The maximum allowed backoff interval when waiting for cryptohome to start.
uint32 kMaxCryptohomeBackoffIntervalMs = 10000u;

// If the data load fails, the initial interval after which the load will be
// retried. Further intervals will exponentially increas by factor 2.
uint32 kInitialCryptohomeBackoffIntervalMs = 200u;

// Calculates the backoff interval that should be used next.
// |backoff| The last backoff interval used.
uint32 GetNextBackoffInterval(uint32 backoff) {
  if (backoff == 0u)
    return kInitialCryptohomeBackoffIntervalMs;
  return backoff * 2;
}

void LoadDataForUser(
    const std::string& user_id,
    uint32 backoff_ms,
    const chromeos::EasyUnlockKeyManager::GetDeviceDataListCallback& callback);

// Callback passed to |LoadDataForUser()|.
// If |LoadDataForUser| function succeeded, it invokes |callback| with the
// results.
// If |LoadDataForUser| failed and further retries are allowed, schedules new
// |LoadDataForUser| call with some backoff. If no further retires are allowed,
// it invokes |callback| with the |LoadDataForUser| results.
void RetryDataLoadOnError(
    const std::string& user_id,
    uint32 backoff_ms,
    const chromeos::EasyUnlockKeyManager::GetDeviceDataListCallback& callback,
    bool success,
    const chromeos::EasyUnlockDeviceKeyDataList& data_list) {
  if (success) {
    callback.Run(success, data_list);
    return;
  }

  uint32 next_backoff_ms = GetNextBackoffInterval(backoff_ms);
  if (next_backoff_ms > kMaxCryptohomeBackoffIntervalMs) {
    callback.Run(false, data_list);
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&LoadDataForUser, user_id, next_backoff_ms, callback),
      base::TimeDelta::FromMilliseconds(next_backoff_ms));
}

// Loads device data list associated with the user's Easy unlock keys.
void LoadDataForUser(
    const std::string& user_id,
    uint32 backoff_ms,
    const chromeos::EasyUnlockKeyManager::GetDeviceDataListCallback& callback) {
  chromeos::EasyUnlockKeyManager* key_manager =
      chromeos::UserSessionManager::GetInstance()->GetEasyUnlockKeyManager();
  DCHECK(key_manager);

  key_manager->GetDeviceDataList(
      chromeos::UserContext(user_id),
      base::Bind(&RetryDataLoadOnError, user_id, backoff_ms, callback));
}

}  // namespace

EasyUnlockServiceSignin::UserData::UserData()
    : state(EasyUnlockServiceSignin::USER_DATA_STATE_INITIAL) {
}

EasyUnlockServiceSignin::UserData::~UserData() {}

EasyUnlockServiceSignin::EasyUnlockServiceSignin(Profile* profile)
    : EasyUnlockService(profile),
      allow_cryptohome_backoff_(true),
      service_active_(false),
      weak_ptr_factory_(this) {
}

EasyUnlockServiceSignin::~EasyUnlockServiceSignin() {
}

EasyUnlockService::Type EasyUnlockServiceSignin::GetType() const {
  return EasyUnlockService::TYPE_SIGNIN;
}

std::string EasyUnlockServiceSignin::GetUserEmail() const {
  return user_id_;
}

void EasyUnlockServiceSignin::LaunchSetup() {
  NOTREACHED();
}

const base::DictionaryValue* EasyUnlockServiceSignin::GetPermitAccess() const {
  return NULL;
}

void EasyUnlockServiceSignin::SetPermitAccess(
    const base::DictionaryValue& permit) {
  NOTREACHED();
}

void EasyUnlockServiceSignin::ClearPermitAccess() {
  NOTREACHED();
}

const base::ListValue* EasyUnlockServiceSignin::GetRemoteDevices() const {
  const UserData* data = FindLoadedDataForCurrentUser();
  if (!data)
    return NULL;
  return &data->remote_devices_value;
}

void EasyUnlockServiceSignin::SetRemoteDevices(
    const base::ListValue& devices) {
  NOTREACHED();
}

void EasyUnlockServiceSignin::RunTurnOffFlow() {
  NOTREACHED();
}

void EasyUnlockServiceSignin::ResetTurnOffFlow() {
  NOTREACHED();
}

EasyUnlockService::TurnOffFlowStatus
    EasyUnlockServiceSignin::GetTurnOffFlowStatus() const {
  return EasyUnlockService::IDLE;
}

std::string EasyUnlockServiceSignin::GetChallenge() const {
  const UserData* data = FindLoadedDataForCurrentUser();
  // TODO(xiyuan): Use correct remote device instead of hard coded first one.
  uint32 device_index = 0;
  if (!data || data->devices.size() <= device_index)
    return std::string();
  return data->devices[device_index].challenge;
}

std::string EasyUnlockServiceSignin::GetWrappedSecret() const {
  const UserData* data = FindLoadedDataForCurrentUser();
  // TODO(xiyuan): Use correct remote device instead of hard coded first one.
  uint32 device_index = 0;
  if (!data || data->devices.size() <= device_index)
    return std::string();
  return data->devices[device_index].wrapped_secret;
}

void EasyUnlockServiceSignin::RecordEasySignInOutcome(
    const std::string& user_id,
    bool success) const {
  DCHECK_EQ(GetUserEmail(), user_id);

  RecordEasyUnlockSigninEvent(
      success ? EASY_UNLOCK_SUCCESS : EASY_UNLOCK_FAILURE);
  DVLOG(1) << "Easy sign-in " << (success ? "success" : "failure");
}

void EasyUnlockServiceSignin::RecordPasswordLoginEvent(
    const std::string& user_id) const {
  // This happens during tests, where a user could log in without the user pod
  // being focused.
  if (GetUserEmail() != user_id)
    return;

  if (!IsEnabled())
    return;

  EasyUnlockAuthEvent event = GetPasswordAuthEvent();
  RecordEasyUnlockSigninEvent(event);
  DVLOG(1) << "Easy Sign-in password login event, event=" << event;
}

void EasyUnlockServiceSignin::StartAutoPairing(
    const AutoPairingResultCallback& callback) {
  NOTREACHED();
}

void EasyUnlockServiceSignin::SetAutoPairingResult(
    bool success,
    const std::string& error) {
  NOTREACHED();
}

void EasyUnlockServiceSignin::InitializeInternal() {
  if (chromeos::LoginState::Get()->IsUserLoggedIn())
    return;

  service_active_ = true;

  chromeos::LoginState::Get()->AddObserver(this);
  ScreenlockBridge* screenlock_bridge = ScreenlockBridge::Get();
  screenlock_bridge->AddObserver(this);
  if (!screenlock_bridge->focused_user_id().empty())
    OnFocusedUserChanged(screenlock_bridge->focused_user_id());
}

void EasyUnlockServiceSignin::ShutdownInternal() {
  if (!service_active_)
    return;
  service_active_ = false;

  weak_ptr_factory_.InvalidateWeakPtrs();
  ScreenlockBridge::Get()->RemoveObserver(this);
  chromeos::LoginState::Get()->RemoveObserver(this);
  STLDeleteContainerPairSecondPointers(user_data_.begin(), user_data_.end());
  user_data_.clear();
}

bool EasyUnlockServiceSignin::IsAllowedInternal() const {
  return service_active_ &&
         !user_id_.empty() &&
         !chromeos::LoginState::Get()->IsUserLoggedIn();
}

void EasyUnlockServiceSignin::OnWillFinalizeUnlock(bool success) {
  // This code path should only be exercised for the lock screen, not for the
  // sign-in screen.
  NOTREACHED();
}

void EasyUnlockServiceSignin::OnScreenDidLock(
    ScreenlockBridge::LockHandler::ScreenType screen_type) {
  // In production code, the screen type should always be the signin screen; but
  // in tests, the screen type might be different.
  if (screen_type != ScreenlockBridge::LockHandler::SIGNIN_SCREEN)
    return;

  // Update initial UI is when the account picker on login screen is ready.
  ShowInitialUserState();
}

void EasyUnlockServiceSignin::OnScreenDidUnlock(
    ScreenlockBridge::LockHandler::ScreenType screen_type) {
  // In production code, the screen type should always be the signin screen; but
  // in tests, the screen type might be different.
  if (screen_type != ScreenlockBridge::LockHandler::SIGNIN_SCREEN)
    return;

  DisableAppWithoutResettingScreenlockState();

  Shutdown();
}

void EasyUnlockServiceSignin::OnFocusedUserChanged(const std::string& user_id) {
  if (user_id_ == user_id)
    return;

  // Setting or clearing the user_id may changed |IsAllowed| value, so in these
  // cases update the app state. Otherwise, it's enough to notify the app the
  // user data has been updated.
  bool should_update_app_state = user_id_.empty() != user_id.empty();
  user_id_ = user_id;

  ResetScreenlockState();
  ShowInitialUserState();

  if (should_update_app_state) {
    UpdateAppState();
  } else {
    NotifyUserUpdated();
  }

  LoadCurrentUserDataIfNeeded();

  // Start loading TPM system token.
  // The system token will be needed to sign a nonce using TPM private key
  // during the sign-in protocol.
  EasyUnlockScreenlockStateHandler::HardlockState hardlock_state;
  if (GetPersistedHardlockState(&hardlock_state) &&
      hardlock_state != EasyUnlockScreenlockStateHandler::NO_PAIRING) {
    chromeos::TPMTokenLoader::Get()->EnsureStarted();
  }
}

void EasyUnlockServiceSignin::LoggedInStateChanged() {
  if (!chromeos::LoginState::Get()->IsUserLoggedIn())
    return;
  DisableAppWithoutResettingScreenlockState();
}

void EasyUnlockServiceSignin::LoadCurrentUserDataIfNeeded() {
  // TODO(xiyuan): Revisit this when adding tests.
  if (!base::SysInfo::IsRunningOnChromeOS())
    return;

  if (user_id_.empty() || !service_active_)
    return;

  std::map<std::string, UserData*>::iterator it = user_data_.find(user_id_);
  if (it == user_data_.end())
    user_data_.insert(std::make_pair(user_id_, new UserData()));

  UserData* data = user_data_[user_id_];

  if (data->state != USER_DATA_STATE_INITIAL)
    return;
  data->state = USER_DATA_STATE_LOADING;

  LoadDataForUser(
      user_id_,
      allow_cryptohome_backoff_ ? 0u : kMaxCryptohomeBackoffIntervalMs,
      base::Bind(&EasyUnlockServiceSignin::OnUserDataLoaded,
                 weak_ptr_factory_.GetWeakPtr(),
                 user_id_));
}

void EasyUnlockServiceSignin::OnUserDataLoaded(
    const std::string& user_id,
    bool success,
    const chromeos::EasyUnlockDeviceKeyDataList& devices) {
  allow_cryptohome_backoff_ = false;

  UserData* data = user_data_[user_id];
  data->state = USER_DATA_STATE_LOADED;
  if (success) {
    data->devices = devices;
    chromeos::EasyUnlockKeyManager::DeviceDataListToRemoteDeviceList(
        user_id, devices, &data->remote_devices_value);

    // User could have a NO_HARDLOCK state but has no remote devices if
    // previous user session shuts down before
    // CheckCryptohomeKeysAndMaybeHardlock finishes. Set NO_PAIRING state
    // and update UI to remove the confusing spinner in this case.
    EasyUnlockScreenlockStateHandler::HardlockState hardlock_state;
    if (devices.empty() &&
        GetPersistedHardlockState(&hardlock_state) &&
        hardlock_state == EasyUnlockScreenlockStateHandler::NO_HARDLOCK) {
      SetHardlockStateForUser(user_id,
                              EasyUnlockScreenlockStateHandler::NO_PAIRING);
    }
  }

  // If the fetched data belongs to the currently focused user, notify the app
  // that it has to refresh it's user data.
  if (user_id == user_id_)
    NotifyUserUpdated();
}

const EasyUnlockServiceSignin::UserData*
    EasyUnlockServiceSignin::FindLoadedDataForCurrentUser() const {
  if (user_id_.empty())
    return NULL;

  std::map<std::string, UserData*>::const_iterator it =
      user_data_.find(user_id_);
  if (it == user_data_.end())
    return NULL;
  if (it->second->state != USER_DATA_STATE_LOADED)
    return NULL;
  return it->second;
}
