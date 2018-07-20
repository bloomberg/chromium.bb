// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"

namespace chromeos {

namespace multidevice_setup {

FakeMultiDeviceSetupClient::FakeMultiDeviceSetupClient() = default;

FakeMultiDeviceSetupClient::~FakeMultiDeviceSetupClient() {
  DCHECK(get_eligible_host_devices_callback_queue_.empty());
  DCHECK(set_host_device_public_key_and_callback_queue_.empty());
  DCHECK(get_host_status_callback_queue_.empty());
  DCHECK(retry_set_host_now_callback_queue_.empty());
  DCHECK(trigger_event_for_debugging_type_and_callback_queue_.empty());
}

void FakeMultiDeviceSetupClient::InvokePendingGetEligibleHostDevicesCallback(
    const cryptauth::RemoteDeviceRefList& eligible_devices) {
  std::move(get_eligible_host_devices_callback_queue_.front())
      .Run(eligible_devices);
  get_eligible_host_devices_callback_queue_.pop();
}

void FakeMultiDeviceSetupClient::InvokePendingSetHostDeviceCallback(
    const std::string& expected_public_key,
    bool success) {
  DCHECK_EQ(expected_public_key,
            set_host_device_public_key_and_callback_queue_.front().first);
  std::move(set_host_device_public_key_and_callback_queue_.front().second)
      .Run(success);
  set_host_device_public_key_and_callback_queue_.pop();
}

void FakeMultiDeviceSetupClient::InvokePendingGetHostStatusCallback(
    mojom::HostStatus host_status,
    const base::Optional<cryptauth::RemoteDeviceRef>& host_device) {
  std::move(get_host_status_callback_queue_.front())
      .Run(host_status, host_device);
  get_host_status_callback_queue_.pop();
}

void FakeMultiDeviceSetupClient::InvokePendingRetrySetHostNowCallback(
    bool success) {
  std::move(retry_set_host_now_callback_queue_.front()).Run(success);
  retry_set_host_now_callback_queue_.pop();
}

void FakeMultiDeviceSetupClient::InvokePendingTriggerEventForDebuggingCallback(
    mojom::EventTypeForDebugging expected_type,
    bool success) {
  DCHECK_EQ(expected_type,
            trigger_event_for_debugging_type_and_callback_queue_.front().first);
  std::move(trigger_event_for_debugging_type_and_callback_queue_.front().second)
      .Run(success);
  trigger_event_for_debugging_type_and_callback_queue_.pop();
}

void FakeMultiDeviceSetupClient::GetEligibleHostDevices(
    GetEligibleHostDevicesCallback callback) {
  get_eligible_host_devices_callback_queue_.push(std::move(callback));
}

void FakeMultiDeviceSetupClient::SetHostDevice(
    const std::string& public_key,
    mojom::MultiDeviceSetup::SetHostDeviceCallback callback) {
  set_host_device_public_key_and_callback_queue_.emplace(public_key,
                                                         std::move(callback));
}

void FakeMultiDeviceSetupClient::RemoveHostDevice() {
  num_remove_host_device_called_++;
}

void FakeMultiDeviceSetupClient::GetHostStatus(GetHostStatusCallback callback) {
  get_host_status_callback_queue_.push(std::move(callback));
}

void FakeMultiDeviceSetupClient::RetrySetHostNow(
    mojom::MultiDeviceSetup::RetrySetHostNowCallback callback) {
  retry_set_host_now_callback_queue_.push(std::move(callback));
}

void FakeMultiDeviceSetupClient::TriggerEventForDebugging(
    mojom::EventTypeForDebugging type,
    mojom::MultiDeviceSetup::TriggerEventForDebuggingCallback callback) {
  trigger_event_for_debugging_type_and_callback_queue_.emplace(
      type, std::move(callback));
}

}  // namespace multidevice_setup

}  // namespace chromeos
