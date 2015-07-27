// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_power_manager_client.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"

namespace chromeos {

FakePowerManagerClient::FakePowerManagerClient()
    : num_request_restart_calls_(0),
      num_request_shutdown_calls_(0),
      num_set_policy_calls_(0),
      num_set_is_projecting_calls_(0),
      num_pending_suspend_readiness_callbacks_(0),
      is_projecting_(false),
      weak_ptr_factory_(this) {
}

FakePowerManagerClient::~FakePowerManagerClient() {
}

void FakePowerManagerClient::Init(dbus::Bus* bus) {
  props_.set_battery_percent(50);
  props_.set_is_calculating_battery_time(false);
  props_.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_DISCHARGING);
  props_.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);
  props_.set_battery_time_to_full_sec(0);
  props_.set_battery_time_to_empty_sec(18000);
}

void FakePowerManagerClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakePowerManagerClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool FakePowerManagerClient::HasObserver(const Observer* observer) const {
  return observers_.HasObserver(observer);
}

void FakePowerManagerClient::SetRenderProcessManagerDelegate(
    base::WeakPtr<RenderProcessManagerDelegate> delegate) {
  render_process_manager_delegate_ = delegate;
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
  // RequestStatusUpdate() calls and notifies the observers
  // asynchronously on a real device. On the fake implementation, we call
  // observers in a posted task to emulate the same behavior.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&FakePowerManagerClient::NotifyObservers,
                            weak_ptr_factory_.GetWeakPtr()));
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

void FakePowerManagerClient::SetPowerSource(const std::string& id) {}

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
  if (render_process_manager_delegate_)
    render_process_manager_delegate_->SuspendImminent();
}

void FakePowerManagerClient::SendSuspendDone() {
  if (render_process_manager_delegate_)
    render_process_manager_delegate_->SuspendDone();

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

void FakePowerManagerClient::UpdatePowerProperties(
    const power_manager::PowerSupplyProperties& power_props) {
  props_ = power_props;
  NotifyObservers();
}

void FakePowerManagerClient::NotifyObservers() {
  FOR_EACH_OBSERVER(Observer, observers_, PowerChanged(props_));
}

void FakePowerManagerClient::HandleSuspendReadiness() {
  CHECK(num_pending_suspend_readiness_callbacks_ > 0);

  --num_pending_suspend_readiness_callbacks_;
}

}  // namespace chromeos
