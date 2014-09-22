// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/easy_unlock_service_signin_chromeos.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_key_manager.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/login/auth/user_context.h"

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
      weak_ptr_factory_(this) {
}

EasyUnlockServiceSignin::~EasyUnlockServiceSignin() {
  STLDeleteContainerPairSecondPointers(user_data_.begin(), user_data_.end());
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

void EasyUnlockServiceSignin::ClearRemoteDevices() {
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

void EasyUnlockServiceSignin::InitializeInternal() {
}

bool EasyUnlockServiceSignin::IsAllowedInternal() {
  return !user_id_.empty() &&
         FindLoadedDataForCurrentUser() &&
         CommandLine::ForCurrentProcess()->HasSwitch(
             chromeos::switches::kEnableEasySignin);
}

void EasyUnlockServiceSignin::LoadCurrentUserDataIfNeeded() {
  if (user_id_.empty() ||
      !CommandLine::ForCurrentProcess()->HasSwitch(
             chromeos::switches::kEnableEasySignin))
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

  UserData* data = user_data_[user_id_];
  data->state = USER_DATA_STATE_LOADED;
  if (success) {
    data->devices = devices;
    chromeos::EasyUnlockKeyManager::DeviceDataListToRemoteDeviceList(
        user_id, devices, &data->remote_devices_value);
  }
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
