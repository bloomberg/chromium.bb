// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/proximity_auth_system.h"

#include "base/threading/thread_task_runner_handle.h"
#include "components/proximity_auth/logging/logging.h"
#include "components/proximity_auth/proximity_auth_client.h"
#include "components/proximity_auth/remote_device_life_cycle_impl.h"
#include "components/proximity_auth/unlock_manager.h"

namespace proximity_auth {

namespace {

// The time to wait after the device wakes up before beginning to connect to the
// remote device.
const int kWakeUpTimeoutSeconds = 2;

}  // namespace

ProximityAuthSystem::ProximityAuthSystem(
    ScreenlockType screenlock_type,
    ProximityAuthClient* proximity_auth_client)
    : proximity_auth_client_(proximity_auth_client),
      unlock_manager_(
          new UnlockManager(screenlock_type, proximity_auth_client)),
      suspended_(false),
      started_(false),
      weak_ptr_factory_(this) {}

ProximityAuthSystem::~ProximityAuthSystem() {
  ScreenlockBridge::Get()->RemoveObserver(this);
  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
}

void ProximityAuthSystem::Start() {
  if (started_)
    return;
  started_ = true;
  ScreenlockBridge::Get()->AddObserver(this);
  const AccountId& focused_account_id =
      ScreenlockBridge::Get()->focused_account_id();
  if (focused_account_id.is_valid())
    OnFocusedUserChanged(focused_account_id);
}

void ProximityAuthSystem::Stop() {
  if (!started_)
    return;
  started_ = false;
  ScreenlockBridge::Get()->RemoveObserver(this);
  OnFocusedUserChanged(EmptyAccountId());
}

void ProximityAuthSystem::SetRemoteDevicesForUser(
    const AccountId& account_id,
    const cryptauth::RemoteDeviceList& remote_devices) {
  remote_devices_map_[account_id] = remote_devices;
  if (started_) {
    const AccountId& focused_account_id =
        ScreenlockBridge::Get()->focused_account_id();
    if (focused_account_id.is_valid())
      OnFocusedUserChanged(focused_account_id);
  }
}

cryptauth::RemoteDeviceList ProximityAuthSystem::GetRemoteDevicesForUser(
    const AccountId& account_id) const {
  if (remote_devices_map_.find(account_id) == remote_devices_map_.end())
    return cryptauth::RemoteDeviceList();
  return remote_devices_map_.at(account_id);
}

void ProximityAuthSystem::OnAuthAttempted(const AccountId& /* account_id */) {
  // TODO(tengs): There is no reason to pass the |account_id| argument anymore.
  unlock_manager_->OnAuthAttempted(ScreenlockBridge::LockHandler::USER_CLICK);
}

void ProximityAuthSystem::OnSuspend() {
  PA_LOG(INFO) << "Preparing for device suspension.";
  DCHECK(!suspended_);
  suspended_ = true;
  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
  remote_device_life_cycle_.reset();
}

void ProximityAuthSystem::OnSuspendDone() {
  PA_LOG(INFO) << "Device resumed from suspension.";
  DCHECK(suspended_);

  // TODO(tengs): On ChromeOS, the system's Bluetooth adapter is invalidated
  // when the system suspends. However, Chrome does not receive this
  // notification until a second or so after the system wakes up. That means
  // using the adapter during this time will be problematic, so we wait instead.
  // See crbug.com/537057.
  proximity_auth_client_->UpdateScreenlockState(
      ScreenlockState::BLUETOOTH_CONNECTING);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&ProximityAuthSystem::ResumeAfterWakeUpTimeout,
                            weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kWakeUpTimeoutSeconds));
}

void ProximityAuthSystem::ResumeAfterWakeUpTimeout() {
  PA_LOG(INFO) << "Resume after suspend";
  suspended_ = false;

  if (!ScreenlockBridge::Get()->IsLocked()) {
    PA_LOG(INFO) << "Suspend done, but no lock screen.";
  } else if (!started_) {
    PA_LOG(INFO) << "Suspend done, but not system started.";
  } else {
    OnFocusedUserChanged(ScreenlockBridge::Get()->focused_account_id());
  }
}

void ProximityAuthSystem::OnLifeCycleStateChanged(
    RemoteDeviceLifeCycle::State old_state,
    RemoteDeviceLifeCycle::State new_state) {
  unlock_manager_->OnLifeCycleStateChanged();
}

void ProximityAuthSystem::OnScreenDidLock(
    ScreenlockBridge::LockHandler::ScreenType screen_type) {
  OnFocusedUserChanged(ScreenlockBridge::Get()->focused_account_id());
}

void ProximityAuthSystem::OnScreenDidUnlock(
    ScreenlockBridge::LockHandler::ScreenType screen_type) {
  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
  remote_device_life_cycle_.reset();
}

void ProximityAuthSystem::OnFocusedUserChanged(const AccountId& account_id) {
  // Update the current RemoteDeviceLifeCycle to the focused user.
  if (account_id.is_valid() && remote_device_life_cycle_ &&
      remote_device_life_cycle_->GetRemoteDevice().user_id !=
          account_id.GetUserEmail()) {
    PA_LOG(INFO) << "Focused user changed, destroying life cycle for "
                 << account_id.Serialize() << ".";
    unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
    remote_device_life_cycle_.reset();
  }

  if (remote_devices_map_.find(account_id) == remote_devices_map_.end() ||
      remote_devices_map_[account_id].size() == 0) {
    PA_LOG(INFO) << "User " << account_id.Serialize()
                 << " does not have a RemoteDevice.";
    return;
  }

  // TODO(tengs): We currently assume each user has only one RemoteDevice, so we
  // can simply take the first item in the list.
  cryptauth::RemoteDevice remote_device = remote_devices_map_[account_id][0];
  if (!suspended_) {
    PA_LOG(INFO) << "Creating RemoteDeviceLifeCycle for focused user: "
                 << account_id.Serialize();
    remote_device_life_cycle_.reset(
        new RemoteDeviceLifeCycleImpl(remote_device, proximity_auth_client_));
    unlock_manager_->SetRemoteDeviceLifeCycle(remote_device_life_cycle_.get());
    remote_device_life_cycle_->AddObserver(this);
    remote_device_life_cycle_->Start();
  }
}

}  // proximity_auth
