// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_cec_service_client.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/dbus_method_call_status.h"

namespace chromeos {

FakeCecServiceClient::FakeCecServiceClient() = default;
FakeCecServiceClient::~FakeCecServiceClient() = default;

void FakeCecServiceClient::SendStandBy() {
  stand_by_call_count_++;
  SetDisplayPowerState(PowerState::kStandBy);
}

void FakeCecServiceClient::SendWakeUp() {
  wake_up_call_count_++;
  SetDisplayPowerState(PowerState::kOn);
}

void FakeCecServiceClient::QueryDisplayCecPowerState(
    CecServiceClient::PowerStateCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), tv_power_states_));
}

void FakeCecServiceClient::Init(dbus::Bus* bus) {}

void FakeCecServiceClient::SetDisplayPowerState(PowerState new_state) {
  for (size_t i = 0; i < tv_power_states_.size(); i++) {
    tv_power_states_[i] = new_state;
  }
}

}  // namespace chromeos
