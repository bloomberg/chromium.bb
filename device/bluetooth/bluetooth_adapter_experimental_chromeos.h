// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_EXPERIMENTAL_CHROMEOS_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_EXPERIMENTAL_CHROMEOS_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/experimental_bluetooth_adapter_client.h"
#include "chromeos/dbus/experimental_bluetooth_device_client.h"
#include "chromeos/dbus/experimental_bluetooth_input_client.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace device {

class BluetoothAdapterFactory;

}  // namespace device

namespace chromeos {

class BluetoothDeviceExperimentalChromeOS;
class BluetoothExperimentalChromeOSTest;

// The BluetoothAdapterExperimentalChromeOS class is an alternate implementation
// of BluetoothAdapter for the Chrome OS platform using the Bluetooth Smart
// capable backend. It will become the sole implementation for Chrome OS, and
// be renamed to BluetoothAdapterChromeOS, once the backend is switched,
class BluetoothAdapterExperimentalChromeOS
    : public device::BluetoothAdapter,
      private chromeos::ExperimentalBluetoothAdapterClient::Observer,
      private chromeos::ExperimentalBluetoothDeviceClient::Observer,
      private chromeos::ExperimentalBluetoothInputClient::Observer {
 public:
  // BluetoothAdapter override
  virtual void AddObserver(
      device::BluetoothAdapter::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(
      device::BluetoothAdapter::Observer* observer) OVERRIDE;
  virtual std::string GetAddress() const OVERRIDE;
  virtual std::string GetName() const OVERRIDE;
  virtual bool IsInitialized() const OVERRIDE;
  virtual bool IsPresent() const OVERRIDE;
  virtual bool IsPowered() const OVERRIDE;
  virtual void SetPowered(
      bool powered,
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
  friend class BluetoothDeviceExperimentalChromeOS;
  friend class BluetoothExperimentalChromeOSTest;

  BluetoothAdapterExperimentalChromeOS();
  virtual ~BluetoothAdapterExperimentalChromeOS();

  // ExperimentalBluetoothAdapterClient::Observer override.
  virtual void AdapterAdded(const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void AdapterRemoved(const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void AdapterPropertyChanged(
      const dbus::ObjectPath& object_path,
      const std::string& property_name) OVERRIDE;

  // ExperimentalBluetoothDeviceClient::Observer override.
  virtual void DeviceAdded(const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void DeviceRemoved(const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void DevicePropertyChanged(const dbus::ObjectPath& object_path,
                                     const std::string& property_name) OVERRIDE;

  // ExperimentalBluetoothInputClient::Observer override.
  virtual void InputPropertyChanged(const dbus::ObjectPath& object_path,
                                    const std::string& property_name) OVERRIDE;

  // Internal method used to locate the device object by object path
  // (the devices map and BluetoothDevice methods are by address)
  BluetoothDeviceExperimentalChromeOS* GetDeviceWithPath(
      const dbus::ObjectPath& object_path);

  // Set the tracked adapter to the one in |object_path|, this object will
  // subsequently operate on that adapter until it is removed.
  void SetAdapter(const dbus::ObjectPath& object_path);

  // Set the adapter name to one chosen from the system information, and method
  // called by dbus:: on completion of the alias property change.
  void SetAdapterName();
  void OnSetAlias(bool success);

  // Remove the currently tracked adapter. IsPresent() will return false after
  // this is called.
  void RemoveAdapter();

  // Announce to observers a change in the adapter state.
  void PoweredChanged(bool powered);
  void DiscoveringChanged(bool discovering);
  void PresentChanged(bool present);

  // Announce to observers a change in device state that is not reflected by
  // its D-Bus properties.
  void NotifyDeviceChanged(BluetoothDeviceExperimentalChromeOS* device);

  // Called by dbus:: on completion of the powered property change.
  void OnSetPowered(const base::Closure& callback,
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
  base::WeakPtrFactory<BluetoothAdapterExperimentalChromeOS> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterExperimentalChromeOS);
};

}  // namespace chromeos

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_EXPERIMENTAL_CHROMEOS_H_
