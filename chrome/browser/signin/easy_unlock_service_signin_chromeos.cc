// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/easy_unlock_service_signin_chromeos.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_key_manager.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/login/auth/user_context.h"

EasyUnlockServiceSignin::EasyUnlockServiceSignin(Profile* profile)
    : EasyUnlockService(profile),
      weak_ptr_factory_(this) {
}

EasyUnlockServiceSignin::~EasyUnlockServiceSignin() {
}

void EasyUnlockServiceSignin::SetAssociatedUser(const std::string& user_id) {
  if (user_id_ == user_id)
    return;

  user_id_ = user_id;
  FetchCryptohomeKeys();
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
  // TODO(tbarzic): Implement this (http://crbug.com/401634).
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
  return remote_devices_value_.get();
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
  // TODO(xiyuan): Use correct remote device instead of hard coded first one.
  size_t device_index = 0;
  return device_index < remote_devices_.size()
             ? remote_devices_[device_index].challenge
             : std::string();
}

void EasyUnlockServiceSignin::InitializeInternal() {
}

bool EasyUnlockServiceSignin::IsAllowedInternal() {
  return !user_id_.empty() && !remote_devices_.empty() &&
         CommandLine::ForCurrentProcess()->HasSwitch(
             chromeos::switches::kEnableEasySignin);
}

void EasyUnlockServiceSignin::FetchCryptohomeKeys() {
  remote_devices_.clear();
  remote_devices_value_.reset();
  if (user_id_.empty()) {
    UpdateAppState();
    return;
  }

  chromeos::EasyUnlockKeyManager* key_manager =
      chromeos::UserSessionManager::GetInstance()->GetEasyUnlockKeyManager();
  DCHECK(key_manager);
  key_manager->GetDeviceDataList(
      chromeos::UserContext(user_id_),
      base::Bind(&EasyUnlockServiceSignin::OnCryptohomeKeysFetched,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EasyUnlockServiceSignin::OnCryptohomeKeysFetched(
    bool success,
    const chromeos::EasyUnlockDeviceKeyDataList& devices) {
  if (!success) {
    LOG(WARNING) << "Easy unlock cryptohome keys not found for user "
                 << user_id_;
    return;
  }

  remote_devices_ = devices;
  remote_devices_value_.reset(new base::ListValue);
  chromeos::EasyUnlockKeyManager::DeviceDataListToRemoteDeviceList(
      remote_devices_, remote_devices_value_.get());

  UpdateAppState();
}
