// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_power_manager_client.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"

namespace chromeos {

namespace {
// Minimum power for a USB power source to be classified as AC.
constexpr double kUsbMinAcWatts = 24;
}  // namespace

FakePowerManagerClient::FakePowerManagerClient() : weak_ptr_factory_(this) {}

FakePowerManagerClient::~FakePowerManagerClient() = default;

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

void FakePowerManagerClient::DecreaseScreenBrightness(bool allow_off) {}

void FakePowerManagerClient::IncreaseScreenBrightness() {}

void FakePowerManagerClient::SetScreenBrightnessPercent(double percent,
                                                        bool gradual) {}

void FakePowerManagerClient::GetScreenBrightnessPercent(
    const GetScreenBrightnessPercentCallback& callback) {}

void FakePowerManagerClient::DecreaseKeyboardBrightness() {}

void FakePowerManagerClient::IncreaseKeyboardBrightness() {}

void FakePowerManagerClient::RequestStatusUpdate() {
  // RequestStatusUpdate() calls and notifies the observers
  // asynchronously on a real device. On the fake implementation, we call
  // observers in a posted task to emulate the same behavior.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&FakePowerManagerClient::NotifyObservers,
                            weak_ptr_factory_.GetWeakPtr()));
}

void FakePowerManagerClient::RequestSuspend() {}

void FakePowerManagerClient::RequestRestart(
    power_manager::RequestRestartReason reason,
    const std::string& description) {
  ++num_request_restart_calls_;
}

void FakePowerManagerClient::RequestShutdown(
    power_manager::RequestShutdownReason reason,
    const std::string& description) {
  ++num_request_shutdown_calls_;
}

void FakePowerManagerClient::NotifyUserActivity(
    power_manager::UserActivityType type) {}

void FakePowerManagerClient::NotifyVideoActivity(bool is_fullscreen) {
  video_activity_reports_.push_back(is_fullscreen);
}

void FakePowerManagerClient::SetPolicy(
    const power_manager::PowerManagementPolicy& policy) {
  policy_ = policy;
  ++num_set_policy_calls_;

  if (power_policy_quit_closure_)
    std::move(power_policy_quit_closure_).Run();
}

void FakePowerManagerClient::SetIsProjecting(bool is_projecting) {
  ++num_set_is_projecting_calls_;
  is_projecting_ = is_projecting;
}

void FakePowerManagerClient::SetPowerSource(const std::string& id) {
  props_.set_external_power_source_id(id);
  props_.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);
  for (const auto& source : props_.available_external_power_source()) {
    if (source.id() == id) {
      props_.set_external_power(
          !source.active_by_default() || source.max_power() < kUsbMinAcWatts
              ? power_manager::PowerSupplyProperties_ExternalPower_USB
              : power_manager::PowerSupplyProperties_ExternalPower_AC);
      break;
    }
  }

  NotifyObservers();
}

void FakePowerManagerClient::SetBacklightsForcedOff(bool forced_off) {
  backlights_forced_off_ = forced_off;
  ++num_set_backlights_forced_off_calls_;
}

void FakePowerManagerClient::GetBacklightsForcedOff(
    const GetBacklightsForcedOffCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, backlights_forced_off_));
}

void FakePowerManagerClient::GetSwitchStates(
    const GetSwitchStatesCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, lid_state_, tablet_mode_));
}

base::Closure FakePowerManagerClient::GetSuspendReadinessCallback() {
  ++num_pending_suspend_readiness_callbacks_;

  return base::Bind(&FakePowerManagerClient::HandleSuspendReadiness,
                    base::Unretained(this));
}

int FakePowerManagerClient::GetNumPendingSuspendReadinessCallbacks() {
  return num_pending_suspend_readiness_callbacks_;
}

bool FakePowerManagerClient::PopVideoActivityReport() {
  CHECK(!video_activity_reports_.empty());
  bool fullscreen = video_activity_reports_.front();
  video_activity_reports_.pop_front();
  return fullscreen;
}

void FakePowerManagerClient::SendSuspendImminent() {
  for (auto& observer : observers_)
    observer.SuspendImminent();
  if (render_process_manager_delegate_)
    render_process_manager_delegate_->SuspendImminent();
}

void FakePowerManagerClient::SendSuspendDone() {
  if (render_process_manager_delegate_)
    render_process_manager_delegate_->SuspendDone();

  for (auto& observer : observers_)
    observer.SuspendDone(base::TimeDelta());
}

void FakePowerManagerClient::SendDarkSuspendImminent() {
  for (auto& observer : observers_)
    observer.DarkSuspendImminent();
}

void FakePowerManagerClient::SendBrightnessChanged(int level,
                                                   bool user_initiated) {
  for (auto& observer : observers_)
    observer.BrightnessChanged(level, user_initiated);
}

void FakePowerManagerClient::SendKeyboardBrightnessChanged(
    int level,
    bool user_initiated) {
  for (auto& observer : observers_)
    observer.KeyboardBrightnessChanged(level, user_initiated);
}

void FakePowerManagerClient::SendPowerButtonEvent(
    bool down,
    const base::TimeTicks& timestamp) {
  for (auto& observer : observers_)
    observer.PowerButtonEventReceived(down, timestamp);
}

void FakePowerManagerClient::SetLidState(LidState state,
                                         const base::TimeTicks& timestamp) {
  lid_state_ = state;
  for (auto& observer : observers_)
    observer.LidEventReceived(state, timestamp);
}

void FakePowerManagerClient::UpdatePowerProperties(
    const power_manager::PowerSupplyProperties& power_props) {
  props_ = power_props;
  NotifyObservers();
}

void FakePowerManagerClient::NotifyObservers() {
  for (auto& observer : observers_)
    observer.PowerChanged(props_);
}

void FakePowerManagerClient::HandleSuspendReadiness() {
  CHECK(num_pending_suspend_readiness_callbacks_ > 0);

  --num_pending_suspend_readiness_callbacks_;
}

void FakePowerManagerClient::SetPowerPolicyQuitClosure(
    base::OnceClosure quit_closure) {
  power_policy_quit_closure_ = std::move(quit_closure);
}

}  // namespace chromeos
