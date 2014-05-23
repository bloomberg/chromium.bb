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
  // |out_services| empty. Returns true, on success. |out_services| must not
  // be NULL. If it is non-empty, then its contents will be cleared.
  typedef std::vector<linked_ptr<api::bluetooth_low_energy::Service> >
      ServiceList;
  bool GetServices(const std::string& device_address,
                   ServiceList* out_services) const;

  // Populates |out_service| based on GATT service with instance ID
  // |instance_id|. Returns true on success. Returns false, if no GATT service
  // with the given ID is known. |out_service| must not be NULL.
  bool GetService(const std::string& instance_id,
                  api::bluetooth_low_energy::Service* out_service) const;

  // Populates |out_services| with the list of GATT services that are included
  // by the GATT service with instance ID |instance_id|. Returns false, if not
  // GATT service with the given ID is known. Returns true, on success. If
  // the given service has no included services, then |out_service| will be
  // empty. |out_service| must not be NULL. If it is non-empty, then its
  // contents will be cleared.
  bool GetIncludedServices(const std::string& instance_id,
                           ServiceList* out_services) const;

  // Returns the list of api::bluetooth_low_energy::Characteristic objects
  // associated with the GATT service with instance ID |instance_id| in
  // |out_characteristics|. Returns false, if no service with the given instance
  // ID is known. If the service is found but it has no characteristics, then
  // returns true and leaves |out_characteristics| empty. Returns true on
  // success. |out_characteristics| must not be NULL and if it is non-empty,
  // then its contents will be cleared.
  typedef std::vector<linked_ptr<api::bluetooth_low_energy::Characteristic> >
      CharacteristicList;
  bool GetCharacteristics(const std::string& instance_id,
                          CharacteristicList* out_characteristics) const;

  // Populates |out_characteristic| based on GATT characteristic with instance
  // ID |instance_id|. Returns true, on success. Returns false, if no GATT
  // characteristic with the given ID is known. |out_characteristic| must not be
  // NULL.
  bool GetCharacteristic(
      const std::string& instance_id,
      api::bluetooth_low_energy::Characteristic* out_characteristic) const;

  // Returns the list of api::bluetooth_low_energy::Descriptor objects
  // associated with the GATT characteristic with instance ID |instance_id| in
  // |out_descriptors|. Returns false, if no characteristic with the given
  // instance ID is known. If the characteristic is found but it has no
  // descriptors, then returns true and leaves |out_descriptors| empty. Returns
  // true on success. |out_descriptors| must not be NULL and if it is non-empty,
  // then its contents will be cleared.
  typedef std::vector<linked_ptr<api::bluetooth_low_energy::Descriptor> >
      DescriptorList;
  bool GetDescriptors(const std::string& instance_id,
                      DescriptorList* out_descriptors) const;

  // Populates |out_descriptor| based on GATT characteristic descriptor with
  // instance ID |instance_id|. Returns true, on success. Returns false, if no
  // GATT descriptor with the given ID is known. |out_descriptor| must not be
  // NULL.
  bool GetDescriptor(
      const std::string& instance_id,
      api::bluetooth_low_energy::Descriptor* out_descriptor) const;

  // Sends a request to read the value of the characteristic with intance ID
  // |instance_id|. Returns false, if no such characteristic is known.
  // Otherwise, returns true and invokes |callback| on success and
  // |error_callback| on failure.
  bool ReadCharacteristicValue(const std::string& instance_id,
                               const base::Closure& callback,
                               const base::Closure& error_callback);

  // Sends a request to write the value of the characteristic with instance ID
  // |instance_id|, with value |value|. Returns false, if no such characteristic
  // is known. Otherwise, returns true and invokes |callback| on success and
  // |error_callback| on failure.
  bool WriteCharacteristicValue(const std::string& instance_id,
                                const std::vector<uint8>& value,
                                const base::Closure& callback,
                                const base::Closure& error_callback);

  // Sends a request to read the value of the descriptor with instance ID
  // |instance_id|. Returns false, if no such descriptor is known. Otherwise,
  // returns true and invokes |callback| on success, and |error_callback| on
  // failure.
  bool ReadDescriptorValue(const std::string& instance_id,
                           const base::Closure& callback,
                           const base::Closure& error_callback);

  // Sends a request to write the value of the descriptor with instance ID
  // |instance_id|, with value |value|. Returns false, if no such descriptor
  // is known. Otherwise, returns true and invokes |callback| on success and
  // |error_callback| on failure.
  bool WriteDescriptorValue(const std::string& instance_id,
                            const std::vector<uint8>& value,
                            const base::Closure& callback,
                            const base::Closure& error_callback);

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
  virtual void GattDescriptorAdded(
      device::BluetoothGattCharacteristic* characteristic,
      device::BluetoothGattDescriptor* descriptor) OVERRIDE;
  virtual void GattDescriptorRemoved(
      device::BluetoothGattCharacteristic* characteristic,
      device::BluetoothGattDescriptor* descriptor) OVERRIDE;
  virtual void GattCharacteristicValueChanged(
      device::BluetoothGattService* service,
      device::BluetoothGattCharacteristic* characteristic,
      const std::vector<uint8>& value) OVERRIDE;
  virtual void GattDescriptorValueChanged(
      device::BluetoothGattCharacteristic* characteristic,
      device::BluetoothGattDescriptor* descriptor,
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

  // Returns a BluetoothGattCharacteristic by its instance ID |instance_id|.
  // Returns NULL, if the characteristic cannot be found.
  device::BluetoothGattCharacteristic* FindCharacteristicById(
      const std::string& instance_id) const;

  // Returns a BluetoothGattDescriptor by its instance ID |instance_id|.
  // Returns NULL, if the descriptor cannot be found.
  device::BluetoothGattDescriptor* FindDescriptorById(
      const std::string& instance_id) const;

  // Called by BluetoothGattCharacteristic and BluetoothGattDescriptor in
  // response to ReadRemoteCharacteristic and ReadRemoteDescriptor.
  void ValueCallback(const base::Closure& callback,
                     const std::vector<uint8>& value);

  // Mapping from instance ids to identifiers of owning instances. The keys are
  // used to identify individual instances of GATT objects and are used by
  // bluetoothLowEnergy API functions to obtain the correct GATT object to
  // operate on. Instance IDs are string identifiers that are returned by the
  // device/bluetooth API, by calling GetIdentifier() on the corresponding
  // device::BluetoothGatt* instance.
  //
  // This mapping is necessary, as GATT object instances can only be obtained
  // from the object that owns it, where raw pointers should not be cached. E.g.
  // to obtain a device::BluetoothGattCharacteristic, it is necessary to obtain
  // a pointer to the associated device::BluetoothDevice, and then to the
  // device::BluetoothGattService that owns the characteristic.
  typedef std::map<std::string, std::string> InstanceIdMap;
  InstanceIdMap service_id_to_device_address_;
  InstanceIdMap chrc_id_to_service_id_;
  InstanceIdMap desc_id_to_chrc_id_;

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
