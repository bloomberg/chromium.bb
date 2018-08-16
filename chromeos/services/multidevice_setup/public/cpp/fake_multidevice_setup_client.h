// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_MULTIDEVICE_SETUP_CLIENT_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_MULTIDEVICE_SETUP_CLIENT_H_

#include <memory>
#include <queue>
#include <string>
#include <tuple>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/cryptauth/remote_device_ref.h"

namespace chromeos {

namespace multidevice_setup {

// Test double implementation of MultiDeviceSetupClient.
class FakeMultiDeviceSetupClient : public MultiDeviceSetupClient {
 public:
  FakeMultiDeviceSetupClient();
  ~FakeMultiDeviceSetupClient() override;

  void InvokePendingGetEligibleHostDevicesCallback(
      const cryptauth::RemoteDeviceRefList& eligible_devices);
  void InvokePendingSetHostDeviceCallback(const std::string& expected_device_id,
                                          bool success);
  void InvokePendingGetHostStatusCallback(
      mojom::HostStatus host_status,
      const base::Optional<cryptauth::RemoteDeviceRef>& host_device);
  void InvokePendingSetFeatureEnabledStateCallback(
      mojom::Feature expected_feature,
      bool expected_enabled,
      bool success);
  void InvokePendingGetFeatureStatesCallback(
      const FeatureStatesMap& feature_states_map);
  void InvokePendingRetrySetHostNowCallback(bool success);
  void InvokePendingTriggerEventForDebuggingCallback(
      mojom::EventTypeForDebugging expected_type,
      bool success);

  size_t num_remove_host_device_called() {
    return num_remove_host_device_called_;
  }

  using MultiDeviceSetupClient::NotifyHostStatusChanged;
  using MultiDeviceSetupClient::NotifyFeatureStateChanged;

 private:
  void GetEligibleHostDevices(GetEligibleHostDevicesCallback callback) override;
  void SetHostDevice(
      const std::string& host_device_id,
      mojom::MultiDeviceSetup::SetHostDeviceCallback callback) override;
  void RemoveHostDevice() override;
  void GetHostStatus(GetHostStatusCallback callback) override;
  void SetFeatureEnabledState(
      mojom::Feature feature,
      bool enabled,
      mojom::MultiDeviceSetup::SetFeatureEnabledStateCallback callback)
      override;
  void GetFeatureStates(
      mojom::MultiDeviceSetup::GetFeatureStatesCallback callback) override;
  void RetrySetHostNow(
      mojom::MultiDeviceSetup::RetrySetHostNowCallback callback) override;
  void TriggerEventForDebugging(
      mojom::EventTypeForDebugging type,
      mojom::MultiDeviceSetup::TriggerEventForDebuggingCallback callback)
      override;

  size_t num_remove_host_device_called_ = 0u;

  std::queue<GetEligibleHostDevicesCallback>
      get_eligible_host_devices_callback_queue_;
  std::queue<
      std::pair<std::string, mojom::MultiDeviceSetup::SetHostDeviceCallback>>
      set_host_device_id_and_callback_queue_;
  std::queue<GetHostStatusCallback> get_host_status_callback_queue_;
  std::queue<
      std::tuple<mojom::Feature,
                 bool,
                 mojom::MultiDeviceSetup::SetFeatureEnabledStateCallback>>
      set_feature_enabled_state_args_queue_;
  std::queue<mojom::MultiDeviceSetup::GetFeatureStatesCallback>
      get_feature_states_args_queue_;
  std::queue<mojom::MultiDeviceSetup::RetrySetHostNowCallback>
      retry_set_host_now_callback_queue_;
  std::queue<
      std::pair<mojom::EventTypeForDebugging,
                mojom::MultiDeviceSetup::TriggerEventForDebuggingCallback>>
      trigger_event_for_debugging_type_and_callback_queue_;

  DISALLOW_COPY_AND_ASSIGN(FakeMultiDeviceSetupClient);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_MULTIDEVICE_SETUP_CLIENT_H_
