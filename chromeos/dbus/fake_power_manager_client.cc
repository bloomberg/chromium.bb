// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/policy.pb.h"

namespace chromeos {

FakePowerManagerClient::FakePowerManagerClient() {
}

FakePowerManagerClient::~FakePowerManagerClient() {
}

void FakePowerManagerClient::AddObserver(Observer* observer) {
}

void FakePowerManagerClient::RequestStatusUpdate(
    UpdateRequestType update_type) {
}

void FakePowerManagerClient::SetPolicy(
    const power_manager::PowerManagementPolicy& policy) {
  policy_ = policy;
}

void FakePowerManagerClient::RequestShutdown() {
}

void FakePowerManagerClient::RequestIdleNotification(int64 threshold_secs) {
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

bool FakePowerManagerClient::HasObserver(Observer* observer) {
  return false;
}

void FakePowerManagerClient::RequestRestart() {
}

void FakePowerManagerClient::IncreaseKeyboardBrightness() {
}

void FakePowerManagerClient::IncreaseScreenBrightness() {
}

void FakePowerManagerClient::NotifyVideoActivity(
    const base::TimeTicks& last_activity_time,
    bool is_fullscreen) {
}

void FakePowerManagerClient::DecreaseKeyboardBrightness() {
}

void FakePowerManagerClient::SetIsProjecting(bool is_projecting) {
}

void FakePowerManagerClient::RemoveObserver(Observer* observer) {
}

void FakePowerManagerClient::NotifyUserActivity() {
}

} // namespace chromeos
