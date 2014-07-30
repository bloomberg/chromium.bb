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

namespace device {

class BluetoothGattNotifySession;

}  // namespace device

namespace extensions {

class BluetoothLowEnergyConnection;
class BluetoothLowEnergyNotifySession;
class Extension;

// The BluetoothLowEnergyEventRouter is used by the bluetoothLowEnergy API to
// interface with the internal Bluetooth API in device/bluetooth.
class BluetoothLowEnergyEventRouter
    : public device::BluetoothAdapter::Observer {
 public:
  explicit BluetoothLowEnergyEventRouter(content::BrowserContext* context);
  virtual ~BluetoothLowEnergyEventRouter();

  // Possible ways that an API method can fail or succeed.
  enum Status {
    kStatusSuccess = 0,
    kStatusErrorPermissionDenied,
    kStatusErrorNotFound,
    kStatusErrorAlreadyConnected,
    kStatusErrorAlreadyNotifying,
    kStatusErrorNotConnected,
    kStatusErrorNotNotifying,
    kStatusErrorInProgress,
    kStatusErrorFailed
  };

  // Error callback is used by asynchronous methods to report failures.
  typedef base::Callback<void(Status)> ErrorCallback;

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

  // Creates a GATT connection to the device with address |device_address| for
  // extension |extension|. The connection is kept alive until the extension is
  // unloaded, the device is removed, or is disconnect by the host subsystem.
  // |error_callback| is called with an error status in case of failure. If
  // |persistent| is true, then the allocated connection resource is persistent
  // across unloads.
  void Connect(bool persistent,
               const Extension* extension,
               const std::string& device_address,
               const base::Closure& callback,
               const ErrorCallback& error_callback);

  // Disconnects the currently open GATT connection of extension |extension| to
  // device with address |device_address|. |error_callback| is called with an
  // error status in case of failure, e.g. if the device is not found or the
  // given
  // extension does not have an open connection to the device.
  void Disconnect(const Extension* extension,
                  const std::string& device_address,
                  const base::Closure& callback,
                  const ErrorCallback& error_callback);

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
  // |instance_id|. |out_service| must not be NULL.
  Status GetService(const std::string& instance_id,
                    api::bluetooth_low_energy::Service* out_service) const;

  // Populates |out_services| with the list of GATT services that are included
  // by the GATT service with instance ID |instance_id|. Returns false, if not
  // GATT service with the given ID is known. If the given service has no
  // included services, then |out_service| will be empty. |out_service| must not
  // be NULL. If it is non-empty, then its contents will be cleared.
  Status GetIncludedServices(const std::string& instance_id,
                             ServiceList* out_services) const;

  // Returns the list of api::bluetooth_low_energy::Characteristic objects
  // associated with the GATT service with instance ID |instance_id| in
  // |out_characteristics|. Returns false, if no service with the given instance
  // ID is known. If the service is found but it has no characteristics, then
  // returns true and leaves |out_characteristics| empty.
  // |out_characteristics| must not be NULL and if it is non-empty,
  // then its contents will be cleared. |extension| is the extension that made
  // the call.
  typedef std::vector<linked_ptr<api::bluetooth_low_energy::Characteristic> >
      CharacteristicList;
  Status GetCharacteristics(const Extension* extension,
                            const std::string& instance_id,
                            CharacteristicList* out_characteristics) const;

  // Populates |out_characteristic| based on GATT characteristic with instance
  // ID |instance_id|. |out_characteristic| must not be NULL. |extension| is the
  // extension that made the call.
  Status GetCharacteristic(
      const Extension* extension,
      const std::string& instance_id,
      api::bluetooth_low_energy::Characteristic* out_characteristic) const;

  // Returns the list of api::bluetooth_low_energy::Descriptor objects
  // associated with the GATT characteristic with instance ID |instance_id| in
  // |out_descriptors|. If the characteristic is found but it has no
  // descriptors, then returns true and leaves |out_descriptors| empty.
  // |out_descriptors| must not be NULL and if it is non-empty,
  // then its contents will be cleared. |extension| is the extension that made
  // the call.
  typedef std::vector<linked_ptr<api::bluetooth_low_energy::Descriptor> >
      DescriptorList;
  Status GetDescriptors(const Extension* extension,
                        const std::string& instance_id,
                        DescriptorList* out_descriptors) const;

  // Populates |out_descriptor| based on GATT characteristic descriptor with
  // instance ID |instance_id|. |out_descriptor| must not be NULL.
  // |extension| is the extension that made the call.
  Status GetDescriptor(
      const Extension* extension,
      const std::string& instance_id,
      api::bluetooth_low_energy::Descriptor* out_descriptor) const;

  // Sends a request to read the value of the characteristic with intance ID
  // |instance_id|. Invokes |callback| on success and |error_callback| on
  // failure. |extension| is the extension that made the call.
  void ReadCharacteristicValue(const Extension* extension,
                               const std::string& instance_id,
                               const base::Closure& callback,
                               const ErrorCallback& error_callback);

  // Sends a request to write the value of the characteristic with instance ID
  // |instance_id|. Invokes |callback| on success and |error_callback| on
  // failure. |extension| is the extension that made the call.
  void WriteCharacteristicValue(const Extension* extension,
                                const std::string& instance_id,
                                const std::vector<uint8>& value,
                                const base::Closure& callback,
                                const ErrorCallback& error_callback);

  // Sends a request to start characteristic notifications from characteristic
  // with instance ID |instance_id|, for extension |extension|. Invokes
  // |callback| on success and |error_callback| on failure. If |persistent| is
  // true, then the allocated connection resource is persistent across unloads.
  void StartCharacteristicNotifications(bool persistent,
                                        const Extension* extension,
                                        const std::string& instance_id,
                                        const base::Closure& callback,
                                        const ErrorCallback& error_callback);

  // Sends a request to stop characteristic notifications from characteristic
  // with instance ID |instance_id|, for extension |extension|. Invokes
  // |callback| on success and |error_callback| on failure.
  void StopCharacteristicNotifications(const Extension* extension,
                                       const std::string& instance_id,
                                       const base::Closure& callback,
                                       const ErrorCallback& error_callback);

  // Sends a request to read the value of the descriptor with instance ID
  // |instance_id|. Invokes |callback| on success and |error_callback| on
  // failure. |extension| is the extension that made the call.
  void ReadDescriptorValue(const Extension* extension,
                           const std::string& instance_id,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback);

  // Sends a request to write the value of the descriptor with instance ID
  // |instance_id|. Invokes |callback| on success and |error_callback| on
  // failure. |extension| is the extension that made the call.
  void WriteDescriptorValue(const Extension* extension,
                            const std::string& instance_id,
                            const std::vector<uint8>& value,
                            const base::Closure& callback,
                            const ErrorCallback& error_callback);

  // Initializes the adapter for testing. Used by unit tests only.
  void SetAdapterForTesting(device::BluetoothAdapter* adapter);

  // device::BluetoothAdapter::Observer overrides.
  virtual void GattServiceAdded(device::BluetoothAdapter* adapter,
                                device::BluetoothDevice* device,
                                device::BluetoothGattService* service) OVERRIDE;
  virtual void GattServiceRemoved(
      device::BluetoothAdapter* adapter,
      device::BluetoothDevice* device,
      device::BluetoothGattService* service) OVERRIDE;
  virtual void GattDiscoveryCompleteForService(
      device::BluetoothAdapter* adapter,
      device::BluetoothGattService* service) OVERRIDE;
  virtual void GattServiceChanged(
      device::BluetoothAdapter* adapter,
      device::BluetoothGattService* service) OVERRIDE;
  virtual void GattCharacteristicAdded(
      device::BluetoothAdapter* adapter,
      device::BluetoothGattCharacteristic* characteristic) OVERRIDE;
  virtual void GattCharacteristicRemoved(
      device::BluetoothAdapter* adapter,
      device::BluetoothGattCharacteristic* characteristic) OVERRIDE;
  virtual void GattDescriptorAdded(
      device::BluetoothAdapter* adapter,
      device::BluetoothGattDescriptor* descriptor) OVERRIDE;
  virtual void GattDescriptorRemoved(
      device::BluetoothAdapter* adapter,
      device::BluetoothGattDescriptor* descriptor) OVERRIDE;
  virtual void GattCharacteristicValueChanged(
      device::BluetoothAdapter* adapter,
      device::BluetoothGattCharacteristic* characteristic,
      const std::vector<uint8>& value) OVERRIDE;
  virtual void GattDescriptorValueChanged(
      device::BluetoothAdapter* adapter,
      device::BluetoothGattDescriptor* descriptor,
      const std::vector<uint8>& value) OVERRIDE;

 private:
  // Called by BluetoothAdapterFactory.
  void OnGetAdapter(const base::Closure& callback,
                    scoped_refptr<device::BluetoothAdapter> adapter);

  // Initializes the identifier for all existing GATT objects and devices.
  // Called by OnGetAdapter and SetAdapterForTesting.
  void InitializeIdentifierMappings();

  // Sends the event named |event_name| to all listeners of that event that
  // have the Bluetooth UUID manifest permission for UUID |uuid| and the
  // "low_energy" manifest permission, with |args| as the argument to that
  // event. If the event involves a characteristic, then |characteristic_id|
  // should be the instance ID of the involved characteristic. Otherwise, an
  // empty string should be passed.
  void DispatchEventToExtensionsWithPermission(
      const std::string& event_name,
      const device::BluetoothUUID& uuid,
      const std::string& characteristic_id,
      scoped_ptr<base::ListValue> args);

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
  void OnValueSuccess(const base::Closure& callback,
                      const std::vector<uint8>& value);

  // Called by BluetoothDevice in response to a call to CreateGattConnection.
  void OnCreateGattConnection(
      bool persistent,
      const std::string& extension_id,
      const std::string& device_address,
      const base::Closure& callback,
      scoped_ptr<device::BluetoothGattConnection> connection);

  // Called by BluetoothGattConnection in response to a call to Disconnect.
  void OnDisconnect(const std::string& extension_id,
                    const std::string& device_address,
                    const base::Closure& callback);

  // Called by BluetoothGattCharacteristic and BluetoothGattDescriptor in
  // case of an error during the read/write operations.
  void OnError(const ErrorCallback& error_callback);

  // Called by BluetoothDevice in response to a call to CreateGattConnection.
  void OnConnectError(const std::string& extension_id,
                      const std::string& device_address,
                      const ErrorCallback& error_callback,
                      device::BluetoothDevice::ConnectErrorCode error_code);

  // Called by BluetoothGattCharacteristic in response to a call to
  // StartNotifySession.
  void OnStartNotifySession(
      bool persistent,
      const std::string& extension_id,
      const std::string& characteristic_id,
      const base::Closure& callback,
      scoped_ptr<device::BluetoothGattNotifySession> session);

  // Called by BluetoothGattCharacteristic in response to a call to
  // StartNotifySession.
  void OnStartNotifySessionError(const std::string& extension_id,
                                 const std::string& characteristic_id,
                                 const ErrorCallback& error_callback);

  // Called by BluetoothGattNotifySession in response to a call to Stop.
  void OnStopNotifySession(const std::string& extension_id,
                           const std::string& characteristic_id,
                           const base::Closure& callback);

  // Finds and returns a BluetoothLowEnergyConnection to device with address
  // |device_address| from the managed API resources for extension with ID
  // |extension_id|.
  BluetoothLowEnergyConnection* FindConnection(
      const std::string& extension_id,
      const std::string& device_address);

  // Removes the connection to device with address |device_address| from the
  // managed API resources for extension with ID |extension_id|. Returns false,
  // if the connection could not be found.
  bool RemoveConnection(const std::string& extension_id,
                        const std::string& device_address);

  // Finds and returns a BluetoothLowEnergyNotifySession associated with
  // characteristic with instance ID |characteristic_id| from the managed API
  // API resources for extension with ID |extension_id|.
  BluetoothLowEnergyNotifySession* FindNotifySession(
      const std::string& extension_id,
      const std::string& characteristic_id);

  // Removes the notify session associated with characteristic with
  // instance ID |characteristic_id| from the managed API resources for
  // extension with ID |extension_id|. Returns false, if the session could
  // not be found.
  bool RemoveNotifySession(const std::string& extension_id,
                           const std::string& characteristic_id);

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

  // Pointer to the current BluetoothAdapter instance. This represents a local
  // Bluetooth adapter of the system.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // Set of extension ID + device addresses to which a connect/disconnect is
  // currently pending.
  std::set<std::string> connecting_devices_;
  std::set<std::string> disconnecting_devices_;

  // Set of extension ID + characteristic ID to which a request to start a
  // notify session is currently pending.
  std::set<std::string> pending_session_calls_;

  // BrowserContext passed during initialization.
  content::BrowserContext* browser_context_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothLowEnergyEventRouter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothLowEnergyEventRouter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_LOW_ENERGY_BLUETOOTH_LOW_ENERGY_EVENT_ROUTER_H_
