// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_MULTIDEVICE_SETUP_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_MULTIDEVICE_SETUP_H_

#include <tuple>
#include <utility>
#include <vector>

#include "chromeos/services/multidevice_setup/multidevice_setup_base.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace chromeos {

namespace multidevice_setup {

// Test MultiDeviceSetup implementation.
class FakeMultiDeviceSetup : public MultiDeviceSetupBase {
 public:
  FakeMultiDeviceSetup();
  ~FakeMultiDeviceSetup() override;

  void BindHandle(mojo::ScopedMessagePipeHandle handle);

  mojom::AccountStatusChangeDelegatePtr& delegate() { return delegate_; }

  std::vector<mojom::HostStatusObserverPtr>& host_status_observers() {
    return host_status_observers_;
  }

  std::vector<mojom::FeatureStateObserverPtr>& feature_state_observers() {
    return feature_state_observers_;
  }

  std::vector<GetEligibleHostDevicesCallback>& get_eligible_hosts_args() {
    return get_eligible_hosts_args_;
  }

  std::vector<std::pair<std::string, SetHostDeviceCallback>>& set_host_args() {
    return set_host_args_;
  }

  size_t num_remove_host_calls() { return num_remove_host_calls_; }

  std::vector<GetHostStatusCallback>& get_host_args() { return get_host_args_; }

  std::vector<std::tuple<mojom::Feature, bool, SetFeatureEnabledStateCallback>>&
  set_feature_enabled_args() {
    return set_feature_enabled_args_;
  }

  std::vector<GetFeatureStatesCallback>& get_feature_states_args() {
    return get_feature_states_args_;
  }

  std::vector<RetrySetHostNowCallback>& retry_set_host_now_args() {
    return retry_set_host_now_args_;
  }

  std::vector<std::pair<mojom::EventTypeForDebugging,
                        TriggerEventForDebuggingCallback>>&
  triggered_debug_events() {
    return triggered_debug_events_;
  }

 private:
  // mojom::MultiDeviceSetup:
  void SetAccountStatusChangeDelegate(
      mojom::AccountStatusChangeDelegatePtr delegate) override;
  void AddHostStatusObserver(mojom::HostStatusObserverPtr observer) override;
  void AddFeatureStateObserver(
      mojom::FeatureStateObserverPtr observer) override;
  void GetEligibleHostDevices(GetEligibleHostDevicesCallback callback) override;
  void SetHostDevice(const std::string& host_device_id,
                     SetHostDeviceCallback callback) override;
  void RemoveHostDevice() override;
  void GetHostStatus(GetHostStatusCallback callback) override;
  void SetFeatureEnabledState(mojom::Feature feature,
                              bool enabled,
                              SetFeatureEnabledStateCallback callback) override;
  void GetFeatureStates(GetFeatureStatesCallback callback) override;
  void RetrySetHostNow(RetrySetHostNowCallback callback) override;
  void TriggerEventForDebugging(
      mojom::EventTypeForDebugging type,
      TriggerEventForDebuggingCallback callback) override;

  mojom::AccountStatusChangeDelegatePtr delegate_;
  std::vector<mojom::HostStatusObserverPtr> host_status_observers_;
  std::vector<mojom::FeatureStateObserverPtr> feature_state_observers_;
  std::vector<GetEligibleHostDevicesCallback> get_eligible_hosts_args_;
  std::vector<std::pair<std::string, SetHostDeviceCallback>> set_host_args_;
  size_t num_remove_host_calls_ = 0u;
  std::vector<GetHostStatusCallback> get_host_args_;
  std::vector<std::tuple<mojom::Feature, bool, SetFeatureEnabledStateCallback>>
      set_feature_enabled_args_;
  std::vector<GetFeatureStatesCallback> get_feature_states_args_;
  std::vector<RetrySetHostNowCallback> retry_set_host_now_args_;
  std::vector<
      std::pair<mojom::EventTypeForDebugging, TriggerEventForDebuggingCallback>>
      triggered_debug_events_;

  mojo::BindingSet<mojom::MultiDeviceSetup> bindings_;

  DISALLOW_COPY_AND_ASSIGN(FakeMultiDeviceSetup);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_MULTIDEVICE_SETUP_H_
