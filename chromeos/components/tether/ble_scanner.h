// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_BLE_SCANNER_H_
#define CHROMEOS_COMPONENTS_BLE_SCANNER_H_

#include <map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/components/tether/local_device_data_provider.h"
#include "components/cryptauth/eid_generator.h"
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
        cryptauth::RemoteDevice remote_device) = 0;
  };

  BleScanner(const LocalDeviceDataProvider* local_device_data_provider);
  ~BleScanner() override;

  virtual bool RegisterScanFilterForDevice(
      const cryptauth::RemoteDevice& remote_device);
  virtual bool UnregisterScanFilterForDevice(
      const cryptauth::RemoteDevice& remote_device);

  bool IsDeviceRegistered(const std::string& device_id);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // device::BluetoothAdapter::Observer
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;
  void DeviceAdded(device::BluetoothAdapter* adapter,
                   device::BluetoothDevice* bluetooth_device) override;
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* bluetooth_device) override;

 protected:
  base::ObserverList<Observer> observer_list_;

 private:
  friend class BleScannerTest;

  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual bool IsBluetoothAdapterAvailable() const = 0;
    virtual void GetAdapter(
        const device::BluetoothAdapterFactory::AdapterCallback& callback) = 0;
    virtual const std::vector<uint8_t>* GetServiceDataForUUID(
        const device::BluetoothUUID& service_uuid,
        device::BluetoothDevice* bluetooth_device) = 0;
  };

  class DelegateImpl : public Delegate {
   public:
    DelegateImpl();
    ~DelegateImpl() override;
    bool IsBluetoothAdapterAvailable() const override;
    void GetAdapter(const device::BluetoothAdapterFactory::AdapterCallback&
                        callback) override;
    const std::vector<uint8_t>* GetServiceDataForUUID(
        const device::BluetoothUUID& service_uuid,
        device::BluetoothDevice* bluetooth_device) override;
  };

  BleScanner(std::unique_ptr<Delegate> delegate,
             const cryptauth::EidGenerator* eid_generator,
             const LocalDeviceDataProvider* local_device_data_provider);

  void UpdateDiscoveryStatus();
  void InitializeBluetoothAdapter();
  void OnAdapterInitialized(scoped_refptr<device::BluetoothAdapter> adapter);
  void StartDiscoverySession();
  void OnDiscoverySessionStarted(
      std::unique_ptr<device::BluetoothDiscoverySession> discovery_session);
  void OnStartDiscoverySessionError();
  void StopDiscoverySession();
  void HandleDeviceUpdated(device::BluetoothDevice* bluetooth_device);
  void CheckForMatchingScanFilters(device::BluetoothDevice* bluetooth_device,
                                   std::string& service_data);

  std::unique_ptr<Delegate> delegate_;

  // |eid_generator_| and |local_device_data_provider_| are not owned by this
  // instance and must outlive it.
  const cryptauth::EidGenerator* eid_generator_;
  const LocalDeviceDataProvider* local_device_data_provider_;

  bool is_initializing_adapter_;
  scoped_refptr<device::BluetoothAdapter> adapter_;

  bool is_initializing_discovery_session_;
  std::unique_ptr<device::BluetoothDiscoverySession> discovery_session_;

  std::vector<cryptauth::RemoteDevice> registered_remote_devices_;

  base::WeakPtrFactory<BleScanner> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BleScanner);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_BLE_SCANNER_H_
