// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_MULTIDEVICE_SETUP_CLIENT_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_MULTIDEVICE_SETUP_CLIENT_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/cryptauth/remote_device_ref.h"

namespace chromeos {

namespace multidevice_setup {

// Provides clients access to the MultiDeviceSetup API.
class MultiDeviceSetupClient {
 public:
  using FeatureStatesMap = base::flat_map<mojom::Feature, mojom::FeatureState>;

  class Observer {
   public:
    // Called whenever the host status changes. If the host status is
    // HostStatus::kNoEligibleHosts or
    // HostStatus::kEligibleHostExistsButNoHostSet, |host_device| is null.
    virtual void OnHostStatusChanged(
        mojom::HostStatus host_status,
        const base::Optional<cryptauth::RemoteDeviceRef>& host_device) {}

    // Called whenever the state of any feature has changed.
    virtual void OnFeatureStatesChanged(
        const FeatureStatesMap& feature_states_map) {}

   protected:
    virtual ~Observer() = default;
  };

  using GetEligibleHostDevicesCallback =
      base::OnceCallback<void(const cryptauth::RemoteDeviceRefList&)>;
  using GetHostStatusCallback = base::OnceCallback<void(
      mojom::HostStatus host_status,
      const base::Optional<cryptauth::RemoteDeviceRef>&)>;

  MultiDeviceSetupClient();
  virtual ~MultiDeviceSetupClient();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  virtual void GetEligibleHostDevices(
      GetEligibleHostDevicesCallback callback) = 0;
  virtual void SetHostDevice(
      const std::string& host_device_id,
      mojom::MultiDeviceSetup::SetHostDeviceCallback callback) = 0;
  virtual void RemoveHostDevice() = 0;
  virtual void GetHostStatus(GetHostStatusCallback callback) = 0;
  virtual void SetFeatureEnabledState(
      mojom::Feature feature,
      bool enabled,
      mojom::MultiDeviceSetup::SetFeatureEnabledStateCallback callback) = 0;
  virtual void GetFeatureStates(
      mojom::MultiDeviceSetup::GetFeatureStatesCallback callback) = 0;
  virtual void RetrySetHostNow(
      mojom::MultiDeviceSetup::RetrySetHostNowCallback callback) = 0;
  virtual void TriggerEventForDebugging(
      mojom::EventTypeForDebugging type,
      mojom::MultiDeviceSetup::TriggerEventForDebuggingCallback callback) = 0;

 protected:
  void NotifyHostStatusChanged(
      mojom::HostStatus host_status,
      const base::Optional<cryptauth::RemoteDeviceRef>& host_device);
  void NotifyFeatureStateChanged(const FeatureStatesMap& feature_states_map);

 private:
  base::ObserverList<Observer>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupClient);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_MULTIDEVICE_SETUP_CLIENT_H_
