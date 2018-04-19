// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_cec_service_client.h"

#include "chromeos/dbus/dbus_method_call_status.h"

namespace chromeos {

FakeCecServiceClient::FakeCecServiceClient() = default;
FakeCecServiceClient::~FakeCecServiceClient() = default;

void FakeCecServiceClient::SendStandBy() {
  stand_by_call_count_++;
  current_state_ = kStandBy;
}

void FakeCecServiceClient::SendWakeUp() {
  wake_up_call_count_++;
  current_state_ = kAwake;
}

int FakeCecServiceClient::stand_by_call_count() {
  return stand_by_call_count_;
}

int FakeCecServiceClient::wake_up_call_count() {
  return wake_up_call_count_;
}

FakeCecServiceClient::CurrentState FakeCecServiceClient::current_state() {
  return current_state_;
}

void FakeCecServiceClient::Init(dbus::Bus* bus) {}

}  // namespace chromeos
