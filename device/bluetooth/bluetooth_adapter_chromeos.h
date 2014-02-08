// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_CHROMEOS_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_CHROMEOS_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/bluetooth_adapter_client.h"
#include "chromeos/dbus/bluetooth_device_client.h"
#include "chromeos/dbus/bluetooth_input_client.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace device {

class BluetoothAdapterFactory;

}  // namespace device

namespace chromeos {

class BluetoothChromeOSTest;
class BluetoothDeviceChromeOS;

// The BluetoothAdapterChromeOS class implements BluetoothAdapter for the
// Chrome OS platform.
class BluetoothAdapterChromeOS
    : public device::BluetoothAdapter,
      private chromeos::BluetoothAdapterClient::Observer,
      private chromeos::BluetoothDeviceClient::Observer,
      private chromeos::BluetoothInputClient::Observer {
 public:
  // BluetoothAdapter override
  virtual void AddObserver(
      device::BluetoothAdapter::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(
      device::BluetoothAdapter::Observer* observer) OVERRIDE;
  virtual std::string GetAddress() const OVERRIDE;
  virtual std::string GetName() const OVERRIDE;
  virtual void SetName(const std::string& name,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) OVERRIDE;
  virtual bool IsInitialized() const OVERRIDE;
  virtual bool IsPresent() const OVERRIDE;
  virtual bool IsPowered() const OVERRIDE;
  virtual void SetPowered(
      bool powered,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual bool IsDiscoverable() const OVERRIDE;
  virtual void SetDiscoverable(
      bool discoverable,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual bool IsDiscovering() const OVERRIDE;
  virtual void StartDiscovering(
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void StopDiscovering(
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void ReadLocalOutOfBandPairingData(
      const device::BluetoothAdapter::BluetoothOutOfBandPairingDataCallback&
          callback,
      const ErrorCallback& error_callback) OVERRIDE;

 private:
  friend class device::BluetoothAdapterFactory;
  friend class BluetoothChromeOSTest;
  friend class BluetoothDeviceChromeOS;
  friend class BluetoothProfileChromeOS;
  friend class BluetoothProfileChromeOSTest;

  BluetoothAdapterChromeOS();
  virtual ~BluetoothAdapterChromeOS();

  // BluetoothAdapterClient::Observer override.
  virtual void AdapterAdded(const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void AdapterRemoved(const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void AdapterPropertyChanged(
      const dbus::ObjectPath& object_path,
      const std::string& property_name) OVERRIDE;

  // BluetoothDeviceClient::Observer override.
  virtual void DeviceAdded(const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void DeviceRemoved(const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void DevicePropertyChanged(const dbus::ObjectPath& object_path,
                                     const std::string& property_name) OVERRIDE;

  // BluetoothInputClient::Observer override.
  virtual void InputPropertyChanged(const dbus::ObjectPath& object_path,
                                    const std::string& property_name) OVERRIDE;

  // Internal method used to locate the device object by object path
  // (the devices map and BluetoothDevice methods are by address)
  BluetoothDeviceChromeOS* GetDeviceWithPath(
      const dbus::ObjectPath& object_path);

  // Set the tracked adapter to the one in |object_path|, this object will
  // subsequently operate on that adapter until it is removed.
  void SetAdapter(const dbus::ObjectPath& object_path);

  // Set the adapter name to one chosen from the system information.
  void SetDefaultAdapterName();

  // Remove the currently tracked adapter. IsPresent() will return false after
  // this is called.
  void RemoveAdapter();

  // Announce to observers a change in the adapter state.
  void PoweredChanged(bool powered);
  void DiscoverableChanged(bool discoverable);
  void DiscoveringChanged(bool discovering);
  void PresentChanged(bool present);

  // Announce to observers a change in device state that is not reflected by
  // its D-Bus properties.
  void NotifyDeviceChanged(BluetoothDeviceChromeOS* device);

  // Called by dbus:: on completion of the discoverable property change.
  void OnSetDiscoverable(const base::Closure& callback,
                         const ErrorCallback& error_callback,
                         bool success);

  // Called by dbus:: on completion of an adapter property change.
  void OnPropertyChangeCompleted(const base::Closure& callback,
                                 const ErrorCallback& error_callback,
                                 bool success);

  // Called by dbus:: on completion of the D-Bus method call to start discovery.
  void OnStartDiscovery(const base::Closure& callback);
  void OnStartDiscoveryError(const ErrorCallback& error_callback,
                             const std::string& error_name,
                             const std::string& error_message);

  // Called by dbus:: on completion of the D-Bus method call to stop discovery.
  void OnStopDiscovery(const base::Closure& callback);
  void OnStopDiscoveryError(const ErrorCallback& error_callback,
                            const std::string& error_name,
                            const std::string& error_message);

  // Object path of the adapter we track.
  dbus::ObjectPath object_path_;

  // List of observers interested in event notifications from us.
  ObserverList<device::BluetoothAdapter::Observer> observers_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdapterChromeOS> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterChromeOS);
};

}  // namespace chromeos

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_CHROMEOS_H_
