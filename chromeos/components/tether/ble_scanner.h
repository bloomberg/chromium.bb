// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_BLE_SCANNER_H_
#define CHROMEOS_COMPONENTS_BLE_SCANNER_H_

#include <map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/cryptauth/foreground_eid_generator.h"
#include "components/cryptauth/local_device_data_provider.h"
#include "components/cryptauth/remote_device.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace device {
class BluetoothDevice;
class BluetoothDiscoverySession;
}

namespace chromeos {

namespace tether {

class BleScanner : public device::BluetoothAdapter::Observer {
 public:
  class Observer {
   public:
    virtual void OnReceivedAdvertisementFromDevice(
        const std::string& device_address,
        const cryptauth::RemoteDevice& remote_device) {}
    virtual void OnDiscoverySessionStateChanged(bool discovery_session_active) {
    }
  };

  BleScanner(
      scoped_refptr<device::BluetoothAdapter> adapter,
      const cryptauth::LocalDeviceDataProvider* local_device_data_provider);
  ~BleScanner() override;

  virtual bool RegisterScanFilterForDevice(
      const cryptauth::RemoteDevice& remote_device);
  virtual bool UnregisterScanFilterForDevice(
      const cryptauth::RemoteDevice& remote_device);

  // A discovery session should be active if at least one device has been
  // registered. However, discovery sessions are started and stopped
  // asynchronously, so these two functions may return different values.
  bool ShouldDiscoverySessionBeActive();
  bool IsDiscoverySessionActive();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // device::BluetoothAdapter::Observer
  void DeviceAdded(device::BluetoothAdapter* adapter,
                   device::BluetoothDevice* bluetooth_device) override;
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* bluetooth_device) override;

 protected:
  void NotifyReceivedAdvertisementFromDevice(
      const std::string& device_address,
      const cryptauth::RemoteDevice& remote_device);
  void NotifyDiscoverySessionStateChanged(bool discovery_session_active);

 private:
  friend class BleScannerTest;

  class ServiceDataProvider {
   public:
    virtual ~ServiceDataProvider() {}
    virtual const std::vector<uint8_t>* GetServiceDataForUUID(
        device::BluetoothDevice* bluetooth_device) = 0;
  };

  class ServiceDataProviderImpl : public ServiceDataProvider {
   public:
    ServiceDataProviderImpl();
    ~ServiceDataProviderImpl() override;
    const std::vector<uint8_t>* GetServiceDataForUUID(
        device::BluetoothDevice* bluetooth_device) override;
  };

  BleScanner(
      std::unique_ptr<ServiceDataProvider> service_data_provider,
      scoped_refptr<device::BluetoothAdapter> adapter,
      std::unique_ptr<cryptauth::ForegroundEidGenerator> eid_generator,
      const cryptauth::LocalDeviceDataProvider* local_device_data_provider);

  bool IsDeviceRegistered(const std::string& device_id);

  void UpdateDiscoveryStatus();

  void EnsureDiscoverySessionActive();
  void OnDiscoverySessionStarted(
      std::unique_ptr<device::BluetoothDiscoverySession> discovery_session);
  void OnStartDiscoverySessionError();

  void EnsureDiscoverySessionNotActive();
  void OnDiscoverySessionStopped();
  void OnStopDiscoverySessionError();

  void HandleDeviceUpdated(device::BluetoothDevice* bluetooth_device);
  void CheckForMatchingScanFilters(device::BluetoothDevice* bluetooth_device,
                                   std::string& service_data);

  base::ObserverList<Observer> observer_list_;

  std::unique_ptr<ServiceDataProvider> service_data_provider_;

  scoped_refptr<device::BluetoothAdapter> adapter_;

  std::unique_ptr<cryptauth::ForegroundEidGenerator> eid_generator_;
  // |local_device_data_provider_| is not owned by this instance and must
  // outlive it.
  const cryptauth::LocalDeviceDataProvider* local_device_data_provider_;

  bool is_initializing_discovery_session_ = false;
  bool is_stopping_discovery_session_ = false;
  std::unique_ptr<device::BluetoothDiscoverySession> discovery_session_;

  std::vector<cryptauth::RemoteDevice> registered_remote_devices_;

  base::WeakPtrFactory<BleScanner> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BleScanner);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_BLE_SCANNER_H_
