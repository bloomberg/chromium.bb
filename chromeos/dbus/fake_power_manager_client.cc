// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/policy.pb.h"

namespace chromeos {

FakePowerManagerClient::FakePowerManagerClient()
    : num_request_restart_calls_(0),
      num_set_policy_calls_(0),
      num_set_is_projecting_calls_(0),
      is_projecting_(false) {
}

FakePowerManagerClient::~FakePowerManagerClient() {
}

void FakePowerManagerClient::Init(dbus::Bus* bus) {
}

void FakePowerManagerClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakePowerManagerClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FakePowerManagerClient::RequestStatusUpdate() {
}

void FakePowerManagerClient::SetPolicy(
    const power_manager::PowerManagementPolicy& policy) {
  policy_ = policy;
  ++num_set_policy_calls_;
}

void FakePowerManagerClient::RequestShutdown() {
}

void FakePowerManagerClient::DecreaseScreenBrightness(bool allow_off) {
}

void FakePowerManagerClient::SetScreenBrightnessPercent(double percent,
                                                        bool gradual) {
}

void FakePowerManagerClient::GetScreenBrightnessPercent(
    const GetScreenBrightnessPercentCallback& callback) {
}

base::Closure FakePowerManagerClient::GetSuspendReadinessCallback() {
  return base::Closure();
}

int FakePowerManagerClient::GetNumPendingSuspendReadinessCallbacks() {
  return 0;
}

bool FakePowerManagerClient::HasObserver(Observer* observer) {
  return false;
}

void FakePowerManagerClient::RequestRestart() {
  ++num_request_restart_calls_;
}

void FakePowerManagerClient::IncreaseKeyboardBrightness() {
}

void FakePowerManagerClient::IncreaseScreenBrightness() {
}

void FakePowerManagerClient::NotifyVideoActivity(bool is_fullscreen) {
}

void FakePowerManagerClient::DecreaseKeyboardBrightness() {
}

void FakePowerManagerClient::SetIsProjecting(bool is_projecting) {
  ++num_set_is_projecting_calls_;
  is_projecting_ = is_projecting;
}

void FakePowerManagerClient::NotifyUserActivity(
    power_manager::UserActivityType type) {
}

void FakePowerManagerClient::SendSuspendImminent() {
  FOR_EACH_OBSERVER(Observer, observers_, SuspendImminent());
}

void FakePowerManagerClient::SendSuspendStateChanged(
    const power_manager::SuspendState& suspend_state) {
  base::Time wall_time =
      base::Time::FromInternalValue(suspend_state.wall_time());
  switch (suspend_state.type()) {
    case power_manager::SuspendState_Type_SUSPEND_TO_MEMORY:
      last_suspend_wall_time_ = wall_time;
      break;
    case power_manager::SuspendState_Type_RESUME:
      FOR_EACH_OBSERVER(Observer, observers_,
                        SystemResumed(wall_time - last_suspend_wall_time_));
      break;
  }
}

} // namespace chromeos
