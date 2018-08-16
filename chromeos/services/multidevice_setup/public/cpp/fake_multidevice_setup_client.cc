// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"

namespace chromeos {

namespace multidevice_setup {

FakeMultiDeviceSetupClient::FakeMultiDeviceSetupClient() = default;

FakeMultiDeviceSetupClient::~FakeMultiDeviceSetupClient() {
  DCHECK(get_eligible_host_devices_callback_queue_.empty());
  DCHECK(set_host_device_id_and_callback_queue_.empty());
  DCHECK(get_host_status_callback_queue_.empty());
  DCHECK(set_feature_enabled_state_args_queue_.empty());
  DCHECK(get_feature_states_args_queue_.empty());
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
    const std::string& expected_device_id,
    bool success) {
  DCHECK_EQ(expected_device_id,
            set_host_device_id_and_callback_queue_.front().first);
  std::move(set_host_device_id_and_callback_queue_.front().second).Run(success);
  set_host_device_id_and_callback_queue_.pop();
}

void FakeMultiDeviceSetupClient::InvokePendingGetHostStatusCallback(
    mojom::HostStatus host_status,
    const base::Optional<cryptauth::RemoteDeviceRef>& host_device) {
  std::move(get_host_status_callback_queue_.front())
      .Run(host_status, host_device);
  get_host_status_callback_queue_.pop();
}

void FakeMultiDeviceSetupClient::InvokePendingSetFeatureEnabledStateCallback(
    mojom::Feature expected_feature,
    bool expected_enabled,
    bool success) {
  auto& tuple = set_feature_enabled_state_args_queue_.front();
  DCHECK_EQ(expected_feature, std::get<0>(tuple));
  DCHECK_EQ(expected_enabled, std::get<1>(tuple));
  std::move(std::get<2>(tuple)).Run(success);
  set_feature_enabled_state_args_queue_.pop();
}

void FakeMultiDeviceSetupClient::InvokePendingGetFeatureStatesCallback(
    const FeatureStatesMap& feature_states_map) {
  std::move(get_feature_states_args_queue_.front()).Run(feature_states_map);
  get_feature_states_args_queue_.pop();
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
    const std::string& host_device_id,
    mojom::MultiDeviceSetup::SetHostDeviceCallback callback) {
  set_host_device_id_and_callback_queue_.emplace(host_device_id,
                                                 std::move(callback));
}

void FakeMultiDeviceSetupClient::RemoveHostDevice() {
  num_remove_host_device_called_++;
}

void FakeMultiDeviceSetupClient::GetHostStatus(GetHostStatusCallback callback) {
  get_host_status_callback_queue_.push(std::move(callback));
}

void FakeMultiDeviceSetupClient::SetFeatureEnabledState(
    mojom::Feature feature,
    bool enabled,
    mojom::MultiDeviceSetup::SetFeatureEnabledStateCallback callback) {
  set_feature_enabled_state_args_queue_.emplace(feature, enabled,
                                                std::move(callback));
}

void FakeMultiDeviceSetupClient::GetFeatureStates(
    mojom::MultiDeviceSetup::GetFeatureStatesCallback callback) {
  get_feature_states_args_queue_.emplace(std::move(callback));
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
