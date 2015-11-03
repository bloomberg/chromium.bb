// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/proximity_auth_system.h"

#include "base/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "components/proximity_auth/logging/logging.h"
#include "components/proximity_auth/proximity_auth_client.h"
#include "components/proximity_auth/proximity_monitor_impl.h"
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
    RemoteDevice remote_device,
    ProximityAuthClient* proximity_auth_client)
    : remote_device_(remote_device),
      proximity_auth_client_(proximity_auth_client),
      unlock_manager_(new UnlockManager(
          screenlock_type,
          make_scoped_ptr<ProximityMonitor>(new ProximityMonitorImpl(
              remote_device,
              make_scoped_ptr(new base::DefaultTickClock()))),
          proximity_auth_client)),
      suspended_(false),
      weak_ptr_factory_(this) {}

ProximityAuthSystem::~ProximityAuthSystem() {
  ScreenlockBridge::Get()->RemoveObserver(this);
  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
}

void ProximityAuthSystem::Start() {
  ScreenlockBridge::Get()->AddObserver(this);
  if (remote_device_.user_id == ScreenlockBridge::Get()->focused_user_id())
    OnFocusedUserChanged(ScreenlockBridge::Get()->focused_user_id());
}

void ProximityAuthSystem::OnAuthAttempted(const std::string& user_id) {
  // TODO(tengs): There is no reason to pass the |user_id| argument anymore.
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
  if (ScreenlockBridge::Get()->IsLocked())
    OnFocusedUserChanged(ScreenlockBridge::Get()->focused_user_id());
}

void ProximityAuthSystem::OnLifeCycleStateChanged(
    RemoteDeviceLifeCycle::State old_state,
    RemoteDeviceLifeCycle::State new_state) {
  unlock_manager_->OnLifeCycleStateChanged();
}

void ProximityAuthSystem::OnScreenDidLock(
    ScreenlockBridge::LockHandler::ScreenType screen_type) {
  OnFocusedUserChanged(ScreenlockBridge::Get()->focused_user_id());
}

void ProximityAuthSystem::OnScreenDidUnlock(
    ScreenlockBridge::LockHandler::ScreenType screen_type) {
  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
  remote_device_life_cycle_.reset();
}

void ProximityAuthSystem::OnFocusedUserChanged(const std::string& user_id) {
  if (!user_id.empty() && remote_device_.user_id != user_id) {
    PA_LOG(INFO) << "Different user focused, destroying RemoteDeviceLifeCycle.";
    unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
    remote_device_life_cycle_.reset();
  } else if (!remote_device_life_cycle_ && !suspended_) {
    PA_LOG(INFO) << "Creating RemoteDeviceLifeCycle for focused user.";
    remote_device_life_cycle_.reset(
        new RemoteDeviceLifeCycleImpl(remote_device_, proximity_auth_client_));
    unlock_manager_->SetRemoteDeviceLifeCycle(remote_device_life_cycle_.get());
    remote_device_life_cycle_->AddObserver(this);
    remote_device_life_cycle_->Start();
  }
}

}  // proximity_auth
