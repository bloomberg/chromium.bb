// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_CHROMEOS_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_CHROMEOS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/observer_list.h"
#include "chromeos/dbus/bluetooth_adapter_client.h"
#include "chromeos/dbus/bluetooth_device_client.h"
#include "chromeos/dbus/bluetooth_manager_client.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace device {

class BluetoothAdapterFactory;
class MockBluetoothAdapter;
struct BluetoothOutOfBandPairingData;

}  // namespace device

namespace chromeos {

class BluetoothDeviceChromeOS;

// The BluetoothAdapterChromeOS class is an implementation of BluetoothAdapter
// for Chrome OS platform.
class BluetoothAdapterChromeOS
    : public device::BluetoothAdapter,
      public BluetoothManagerClient::Observer,
      public BluetoothAdapterClient::Observer,
      public BluetoothDeviceClient::Observer {
 public:
  // BluetoothAdapter override
  virtual void AddObserver(
      device::BluetoothAdapter::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(
      device::BluetoothAdapter::Observer* observer) OVERRIDE;
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
  friend class BluetoothDeviceChromeOS;
  friend class device::BluetoothAdapterFactory;
  friend class device::MockBluetoothAdapter;

  BluetoothAdapterChromeOS();
  virtual ~BluetoothAdapterChromeOS();

  // Obtains the default adapter object path from the Bluetooth Daemon
  // and tracks future changes to it.
  void TrackDefaultAdapter();

  // Called by dbus:: in response to the method call sent by DefaultAdapter().
  // |object_path| will contain the dbus object path of the requested adapter
  // when |success| is true.
  void AdapterCallback(const dbus::ObjectPath& adapter_path, bool success);

  // BluetoothManagerClient::Observer override.
  //
  // Called when the default local bluetooth adapter changes.
  // |object_path| is the dbus object path of the new default adapter.
  // Not called if all adapters are removed.
  virtual void DefaultAdapterChanged(const dbus::ObjectPath& adapter_path)
      OVERRIDE;

  // BluetoothManagerClient::Observer override.
  //
  // Called when a local bluetooth adapter is removed.
  // |object_path| is the dbus object path of the adapter.
  virtual void AdapterRemoved(const dbus::ObjectPath& adapter_path) OVERRIDE;

  // Changes the tracked adapter to the dbus object path |adapter_path|,
  // clearing information from the previously tracked adapter and updating
  // to the new adapter.
  void ChangeAdapter(const dbus::ObjectPath& adapter_path);

  // Clears the tracked adapter information.
  void RemoveAdapter();

  // Called by dbus:: in response to the method call send by SetPowered().
  // |callback| and |error_callback| are the callbacks passed to SetPowered().
  void OnSetPowered(const base::Closure& callback,
                    const ErrorCallback& error_callback,
                    bool success);

  // Updates the tracked state of the adapter's radio power to |powered|
  // and notifies observers. Called on receipt of a property changed signal,
  // and directly using values obtained from properties.
  void PoweredChanged(bool powered);

  // Called by BluetoothAdapterClient in response to the method call sent
  // by StartDiscovering(), |callback| and |error_callback| are the callbacks
  // provided to that method.
  void OnStartDiscovery(const base::Closure& callback,
                        const ErrorCallback& error_callback,
                        const dbus::ObjectPath& adapter_path,
                        bool success);

  // Called by BluetoothAdapterClient in response to the method call sent
  // by StopDiscovering(), |callback| and |error_callback| are the callbacks
  // provided to that method.
  void OnStopDiscovery(const base::Closure& callback,
                       const ErrorCallback& error_callback,
                       const dbus::ObjectPath& adapter_path,
                       bool success);

  // Updates the tracked state of the adapter's discovering state to
  // |discovering| and notifies observers. Called on receipt of a property
  // changed signal, and directly using values obtained from properties.
  void DiscoveringChanged(bool discovering);

  // Called by dbus:: in response to the ReadLocalData method call.
  void OnReadLocalData(
      const device::BluetoothAdapter::BluetoothOutOfBandPairingDataCallback&
          callback,
      const ErrorCallback& error_callback,
      const device::BluetoothOutOfBandPairingData& data,
      bool success);

  // BluetoothAdapterClient::Observer override.
  //
  // Called when the adapter with object path |adapter_path| has a
  // change in value of the property named |property_name|.
  virtual void AdapterPropertyChanged(const dbus::ObjectPath& adapter_path,
                                      const std::string& property_name)
      OVERRIDE;

  // BluetoothDeviceClient::Observer override.
  //
  // Called when the device with object path |device_path| has a
  // change in value of the property named |property_name|.
  virtual void DevicePropertyChanged(const dbus::ObjectPath& device_path,
                                     const std::string& property_name)
      OVERRIDE;

  // Updates information on the device with object path |device_path|,
  // adding it to the |devices_| map if not already present.
  void UpdateDevice(const dbus::ObjectPath& device_path);

  // Clears the |devices_| list, notifying obsevers of the device removal.
  void ClearDevices();

  // BluetoothAdapterClient::Observer override.
  //
  // Called when the adapter with object path |object_path| has a
  // new known device with object path |object_path|.
  virtual void DeviceCreated(const dbus::ObjectPath& adapter_path,
                             const dbus::ObjectPath& device_path) OVERRIDE;

  // BluetoothAdapterClient::Observer override.
  //
  // Called when the adapter with object path |object_path| removes
  // the known device with object path |object_path|.
  virtual void DeviceRemoved(const dbus::ObjectPath& adapter_path,
                             const dbus::ObjectPath& device_path) OVERRIDE;

  // Updates the adapter |devices_| list, adding or updating devices using
  // the object paths in the|devices| list. This doesn't remove devices,
  // relying instead on the DeviceRemoved() signal for that. Called on
  // receipt of a property changed signal, and directly using values obtained
  // from properties.
  void DevicesChanged(const std::vector<dbus::ObjectPath>& devices);

  // Clears discovered devices from the |devices_| list, notifying
  // observers, and leaving only those devices with a dbus object path.
  void ClearDiscoveredDevices();

  // BluetoothAdapterClient::Observer override.
  //
  // Called when the adapter with object path |object_path| discovers
  // a new remote device with address |address| and properties
  // |properties|, there is no device object path until connected.
  //
  // |properties| supports only value() calls, not Get() or Set(), and
  // should be copied if needed.
  virtual void DeviceFound(
        const dbus::ObjectPath& adapter_path,
        const std::string& address,
        const BluetoothDeviceClient::Properties& properties) OVERRIDE;

  // BluetoothAdapterClient::Observer override.
  //
  // Called when the adapter with object path |object_path| can no
  // longer communicate with the discovered removed device with
  // address |address|.
  virtual void DeviceDisappeared(const dbus::ObjectPath& object_path,
                                 const std::string& address) OVERRIDE;

  // List of observers interested in event notifications from us.
  ObserverList<device::BluetoothAdapter::Observer> observers_;

  // Object path of adapter for this instance, this is fixed at creation time
  // unless |track_default_| is true in which case we update it to always
  // point at the default adapter.
  bool track_default_;
  dbus::ObjectPath object_path_;

  // Tracked adapter state, cached locally so we only send change notifications
  // to observers on a genuine change.
  bool powered_;
  bool discovering_;

  // Count of callers to StartDiscovering() and StopDiscovering(), used to
  // track whether to clear the discovered devices list on start.
  int discovering_count_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdapterChromeOS> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterChromeOS);
};

}  // namespace chromeos

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_CHROMEOS_H_
