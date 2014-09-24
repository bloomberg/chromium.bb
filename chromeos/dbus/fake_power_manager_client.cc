// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_power_manager_client.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/time/time.h"
#include "chromeos/dbus/power_manager/policy.pb.h"

namespace chromeos {

FakePowerManagerClient::FakePowerManagerClient()
    : num_request_restart_calls_(0),
      num_request_shutdown_calls_(0),
      num_set_policy_calls_(0),
      num_set_is_projecting_calls_(0),
      num_pending_suspend_readiness_callbacks_(0),
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

bool FakePowerManagerClient::HasObserver(Observer* observer) {
  return false;
}

void FakePowerManagerClient::DecreaseScreenBrightness(bool allow_off) {
}

void FakePowerManagerClient::IncreaseScreenBrightness() {
}

void FakePowerManagerClient::SetScreenBrightnessPercent(double percent,
                                                        bool gradual) {
}

void FakePowerManagerClient::GetScreenBrightnessPercent(
    const GetScreenBrightnessPercentCallback& callback) {
}

void FakePowerManagerClient::DecreaseKeyboardBrightness() {
}

void FakePowerManagerClient::IncreaseKeyboardBrightness() {
}

void FakePowerManagerClient::RequestStatusUpdate() {
}

void FakePowerManagerClient::RequestSuspend() {
}

void FakePowerManagerClient::RequestRestart() {
  ++num_request_restart_calls_;
}

void FakePowerManagerClient::RequestShutdown() {
  ++num_request_shutdown_calls_;
}

void FakePowerManagerClient::NotifyUserActivity(
    power_manager::UserActivityType type) {
}

void FakePowerManagerClient::NotifyVideoActivity(bool is_fullscreen) {
}

void FakePowerManagerClient::SetPolicy(
    const power_manager::PowerManagementPolicy& policy) {
  policy_ = policy;
  ++num_set_policy_calls_;
}

void FakePowerManagerClient::SetIsProjecting(bool is_projecting) {
  ++num_set_is_projecting_calls_;
  is_projecting_ = is_projecting;
}

base::Closure FakePowerManagerClient::GetSuspendReadinessCallback() {
  ++num_pending_suspend_readiness_callbacks_;

  return base::Bind(&FakePowerManagerClient::HandleSuspendReadiness,
                    base::Unretained(this));
}

int FakePowerManagerClient::GetNumPendingSuspendReadinessCallbacks() {
  return num_pending_suspend_readiness_callbacks_;
}

void FakePowerManagerClient::SendSuspendImminent() {
  FOR_EACH_OBSERVER(Observer, observers_, SuspendImminent());
}

void FakePowerManagerClient::SendSuspendDone() {
  FOR_EACH_OBSERVER(Observer, observers_, SuspendDone(base::TimeDelta()));
}

void FakePowerManagerClient::SendDarkSuspendImminent() {
  FOR_EACH_OBSERVER(Observer, observers_, DarkSuspendImminent());
}

void FakePowerManagerClient::SendPowerButtonEvent(
    bool down,
    const base::TimeTicks& timestamp) {
  FOR_EACH_OBSERVER(Observer, observers_,
                    PowerButtonEventReceived(down, timestamp));
}

void FakePowerManagerClient::HandleSuspendReadiness() {
  CHECK(num_pending_suspend_readiness_callbacks_ > 0);

  --num_pending_suspend_readiness_callbacks_;
}

} // namespace chromeos
