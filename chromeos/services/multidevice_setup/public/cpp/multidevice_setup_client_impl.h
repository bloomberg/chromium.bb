// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_MULTIDEVICE_SETUP_CLIENT_IMPL_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_MULTIDEVICE_SETUP_CLIENT_IMPL_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/cryptauth/remote_device.h"
#include "components/cryptauth/remote_device_cache.h"
#include "components/cryptauth/remote_device_ref.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace chromeos {

namespace multidevice_setup {

// Concrete implementation of MultiDeviceSetupClient.
class MultiDeviceSetupClientImpl : public MultiDeviceSetupClient,
                                   public mojom::HostStatusObserver {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetInstanceForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<MultiDeviceSetupClient> BuildInstance(
        service_manager::Connector* connector);

   private:
    static Factory* test_factory_;
  };

  ~MultiDeviceSetupClientImpl() override;

  // MultiDeviceSetupClient:
  void GetEligibleHostDevices(GetEligibleHostDevicesCallback callback) override;
  void SetHostDevice(
      const std::string& public_key,
      mojom::MultiDeviceSetup::SetHostDeviceCallback callback) override;
  void RemoveHostDevice() override;
  void GetHostStatus(GetHostStatusCallback callback) override;
  void RetrySetHostNow(
      mojom::MultiDeviceSetup::RetrySetHostNowCallback callback) override;
  void TriggerEventForDebugging(
      mojom::EventTypeForDebugging type,
      mojom::MultiDeviceSetup::TriggerEventForDebuggingCallback callback)
      override;

  // mojom::HostStatusObserver:
  void OnHostStatusChanged(
      mojom::HostStatus host_status,
      const base::Optional<cryptauth::RemoteDevice>& host_device) override;

 private:
  friend class MultiDeviceSetupClientImplTest;

  explicit MultiDeviceSetupClientImpl(service_manager::Connector* connector);

  void OnGetEligibleHostDevicesCompleted(
      GetEligibleHostDevicesCallback callback,
      const cryptauth::RemoteDeviceList& eligible_host_devices);
  void OnGetHostStatusCompleted(
      GetHostStatusCallback callback,
      mojom::HostStatus host_status,
      const base::Optional<cryptauth::RemoteDevice>& host_device);

  mojom::HostStatusObserverPtr GenerateInterfacePtr();

  void FlushForTesting();

  mojom::MultiDeviceSetupPtr multidevice_setup_ptr_;
  mojo::Binding<mojom::HostStatusObserver> binding_;
  std::unique_ptr<cryptauth::RemoteDeviceCache> remote_device_cache_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupClientImpl);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_MULTIDEVICE_SETUP_CLIENT_IMPL_H_
