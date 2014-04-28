// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_LOW_ENERGY_BLUETOOTH_LOW_ENERGY_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_LOW_ENERGY_BLUETOOTH_LOW_ENERGY_EVENT_ROUTER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/extensions/api/bluetooth_low_energy.h"
#include "content/public/browser/notification_observer.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_service.h"

namespace base {

class ListValue;

}  // namespace base

namespace content {

class BrowserContext;

}  // namespace content

namespace extensions {

// The BluetoothLowEnergyEventRouter is used by the bluetoothLowEnergy API to
// interface with the internal Bluetooth API in device/bluetooth.
class BluetoothLowEnergyEventRouter
    : public device::BluetoothAdapter::Observer,
      public device::BluetoothDevice::Observer,
      public device::BluetoothGattService::Observer {
 public:
  explicit BluetoothLowEnergyEventRouter(content::BrowserContext* context);
  virtual ~BluetoothLowEnergyEventRouter();

  // Returns true if Bluetooth is supported on the current platform or if the
  // internal |adapter_| instance has been initialized for testing.
  bool IsBluetoothSupported() const;

  // Obtains a handle on the BluetoothAdapter and invokes |callback|. Returns
  // false, if Bluetooth is not supported. Otherwise, asynchronously initializes
  // it and invokes |callback|. Until the first successful call to this method,
  // none of the methods in this class will succeed and no device::Bluetooth*
  // API events will be observed.
  bool InitializeAdapterAndInvokeCallback(const base::Closure& callback);

  // Returns true, if the BluetoothAdapter was initialized.
  bool HasAdapter() const;

  // Returns the list of api::bluetooth_low_energy::Service objects associated
  // with the Bluetooth device with address |device_address| in |out_services|.
  // Returns false, if no device with the given address is known. If the device
  // is found but it has no GATT services, then returns true and leaves
  // |out_services| as empty. Returns true, on success. |out_services| must not
  // be NULL. If it is non-empty, then its contents will be cleared.
  typedef std::vector<linked_ptr<api::bluetooth_low_energy::Service> >
      ServiceList;
  bool GetServices(const std::string& device_address,
                   ServiceList* out_services) const;

  // Populates |out_service| based on GATT service with instance ID
  // |instance_id|. Returns true on success. Returns false, if no GATT service
  // with the given ID is known. |service| must not be NULL.
  bool GetService(const std::string& instance_id,
                  api::bluetooth_low_energy::Service* out_service) const;

  // Initializes the adapter for testing. Used by unit tests only.
  void SetAdapterForTesting(device::BluetoothAdapter* adapter);

  // device::BluetoothAdapter::Observer overrides.
  virtual void DeviceAdded(device::BluetoothAdapter* adapter,
                           device::BluetoothDevice* device) OVERRIDE;
  virtual void DeviceRemoved(device::BluetoothAdapter* adapter,
                             device::BluetoothDevice* device) OVERRIDE;

  // device::BluetoothDevice::Observer overrides.
  virtual void GattServiceAdded(device::BluetoothDevice* device,
                                device::BluetoothGattService* service) OVERRIDE;
  virtual void GattServiceRemoved(
      device::BluetoothDevice* device,
      device::BluetoothGattService* service) OVERRIDE;

  // device::BluetoothGattService::Observer overrides.
  virtual void GattServiceChanged(
      device::BluetoothGattService* service) OVERRIDE;
  virtual void GattCharacteristicAdded(
      device::BluetoothGattService* service,
      device::BluetoothGattCharacteristic* characteristic) OVERRIDE;
  virtual void GattCharacteristicRemoved(
      device::BluetoothGattService* service,
      device::BluetoothGattCharacteristic* characteristic) OVERRIDE;
  virtual void GattCharacteristicValueChanged(
      device::BluetoothGattService* service,
      device::BluetoothGattCharacteristic* characteristic,
      const std::vector<uint8>& value) OVERRIDE;

 private:
  // Called by BluetoothAdapterFactory.
  void OnGetAdapter(const base::Closure& callback,
                    scoped_refptr<device::BluetoothAdapter> adapter);

  // Initializes the identifier for all existing GATT objects and devices.
  // Called by OnGetAdapter and SetAdapterForTesting.
  void InitializeIdentifierMappings();

  // Returns a BluetoothGattService by its instance ID |instance_id|. Returns
  // NULL, if the service cannot be found.
  device::BluetoothGattService* FindServiceById(
      const std::string& instance_id) const;

  // Meta-data that identifies GATT services, characteristics, and descriptors,
  // for obtaining instances from the BluetoothAdapter.
  struct GattObjectData {
    GattObjectData();
    ~GattObjectData();

    std::string device_address;
    std::string service_id;
    std::string characteristic_id;
    std::string descriptor_id;
  };

  // Mapping from instance ids to GattObjectData. The keys are used to identify
  // individual instances of GATT objects and is used by bluetoothLowEnergy API
  // functions to obtain the correct GATT object to operate on. Instance IDs are
  // string identifiers that are returned by the device/bluetooth API, by
  // calling GetIdentifier() on the corresponding device::BluetoothGatt*
  // instance.
  //
  // This mapping is necessary, as GATT object instances can only be obtained
  // from the object that owns it, where raw pointers should not be cached. E.g.
  // to obtain a device::BluetoothGattCharacteristic, it is necessary to obtain
  // a pointer to the associated device::BluetoothDevice, and then to the
  // device::BluetoothGattService that owns the characteristic.
  typedef std::map<std::string, GattObjectData> InstanceIdToObjectDataMap;
  InstanceIdToObjectDataMap service_ids_to_objects_;

  // Sets of BluetoothDevice and BluetoothGattService objects that are being
  // observed, used to remove the BluetoothLowEnergyEventRouter as an observer
  // during clean up.
  std::set<std::string> observed_devices_;
  std::set<std::string> observed_gatt_services_;

  // Pointer to the current BluetoothAdapter instance. This represents a local
  // Bluetooth adapter of the system.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // BrowserContext passed during initialization.
  content::BrowserContext* browser_context_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothLowEnergyEventRouter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothLowEnergyEventRouter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_LOW_ENERGY_BLUETOOTH_LOW_ENERGY_EVENT_ROUTER_H_
