// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_BLE_SCANNER_IMPL_H_
#define CHROMEOS_COMPONENTS_TETHER_BLE_SCANNER_IMPL_H_

#include <map>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/tether/ble_scanner.h"
#include "components/cryptauth/foreground_eid_generator.h"
#include "components/cryptauth/local_device_data_provider.h"
#include "components/cryptauth/remote_device.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace base {
class TaskRunner;
}  // namespace base

namespace device {
class BluetoothDevice;
class BluetoothDiscoverySession;
}  // namespace device

namespace chromeos {

namespace tether {

class BleSynchronizerBase;
class TetherHostFetcher;

// Concrete BleScanner implementation.
class BleScannerImpl : public BleScanner,
                       public device::BluetoothAdapter::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<BleScanner> NewInstance(
        scoped_refptr<device::BluetoothAdapter> adapter,
        cryptauth::LocalDeviceDataProvider* local_device_data_provider,
        BleSynchronizerBase* ble_synchronizer,
        TetherHostFetcher* tether_host_fetcher);

    static void SetInstanceForTesting(Factory* factory);

   protected:
    virtual std::unique_ptr<BleScanner> BuildInstance(
        scoped_refptr<device::BluetoothAdapter> adapter,
        cryptauth::LocalDeviceDataProvider* local_device_data_provider,
        BleSynchronizerBase* ble_synchronizer,
        TetherHostFetcher* tether_host_fetcher);

   private:
    static Factory* factory_instance_;
  };

  BleScannerImpl(scoped_refptr<device::BluetoothAdapter> adapter,
                 cryptauth::LocalDeviceDataProvider* local_device_data_provider,
                 BleSynchronizerBase* ble_synchronizer,
                 TetherHostFetcher* tether_host_fetcher);
  ~BleScannerImpl() override;

  // BleScanner:
  bool RegisterScanFilterForDevice(const std::string& device_id) override;
  bool UnregisterScanFilterForDevice(const std::string& device_id) override;
  bool ShouldDiscoverySessionBeActive() override;
  bool IsDiscoverySessionActive() override;

 protected:
  // device::BluetoothAdapter::Observer:
  void DeviceAdded(device::BluetoothAdapter* adapter,
                   device::BluetoothDevice* bluetooth_device) override;
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* bluetooth_device) override;

 private:
  friend class BleScannerImplTest;

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

  void SetTestDoubles(
      std::unique_ptr<ServiceDataProvider> service_data_provider,
      std::unique_ptr<cryptauth::ForegroundEidGenerator> eid_generator,
      scoped_refptr<base::TaskRunner> test_task_runner);

  bool IsDeviceRegistered(const std::string& device_id);

  // A discovery session should stay active until it has been stopped. However,
  // due to bugs in Bluetooth code, it is possible for a discovery status to
  // transition to being off without a Stop() call ever succeeding. This
  // function corrects the state of Bluetooth if such a bug occurs.
  void ResetDiscoverySessionIfNotActive();

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
                                   const std::string& service_data);
  void OnIdentifiedHostFetched(
      device::BluetoothDevice* bluetooth_device,
      const std::string& device_id,
      std::unique_ptr<cryptauth::RemoteDevice> identified_device);

  void ScheduleStatusChangeNotification(bool discovery_session_active);

  scoped_refptr<device::BluetoothAdapter> adapter_;
  cryptauth::LocalDeviceDataProvider* local_device_data_provider_;
  BleSynchronizerBase* ble_synchronizer_;
  TetherHostFetcher* tether_host_fetcher_;

  std::unique_ptr<ServiceDataProvider> service_data_provider_;
  std::unique_ptr<cryptauth::ForegroundEidGenerator> eid_generator_;

  std::vector<std::string> registered_remote_device_ids_;

  bool is_initializing_discovery_session_ = false;
  bool is_stopping_discovery_session_ = false;
  scoped_refptr<base::TaskRunner> task_runner_;
  std::unique_ptr<device::BluetoothDiscoverySession> discovery_session_;
  std::unique_ptr<base::WeakPtrFactory<device::BluetoothDiscoverySession>>
      discovery_session_weak_ptr_factory_;

  base::WeakPtrFactory<BleScannerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BleScannerImpl);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_BLE_SCANNER_IMPL_H_
