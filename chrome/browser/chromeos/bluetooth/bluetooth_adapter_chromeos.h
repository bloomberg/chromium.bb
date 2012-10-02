// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_ADAPTER_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_ADAPTER_CHROMEOS_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_adapter.h"
#include "chromeos/dbus/bluetooth_adapter_client.h"
#include "chromeos/dbus/bluetooth_device_client.h"
#include "chromeos/dbus/bluetooth_manager_client.h"
#include "dbus/object_path.h"

namespace chromeos {

class BluetoothDeviceChromeOs;

struct BluetoothOutOfBandPairingData;

// The BluetoothAdapterChromeOs class is an implementation of BluetoothAdapter
// for Chrome OS platform.
class BluetoothAdapterChromeOs : public BluetoothAdapter,
                                 public BluetoothManagerClient::Observer,
                                 public BluetoothAdapterClient::Observer,
                                 public BluetoothDeviceClient::Observer {
 public:
  // BluetoothAdapter override
  virtual void AddObserver(BluetoothAdapter::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(BluetoothAdapter::Observer* observer) OVERRIDE;
  virtual bool IsPresent() const OVERRIDE;
  virtual bool IsPowered() const OVERRIDE;
  virtual void SetPowered(
      bool powered,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual bool IsDiscovering() const OVERRIDE;
  virtual void SetDiscovering(
      bool discovering,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual ConstDeviceList GetDevices() const OVERRIDE;
  virtual BluetoothDevice* GetDevice(const std::string& address) OVERRIDE;
  virtual const BluetoothDevice* GetDevice(
      const std::string& address) const OVERRIDE;
  virtual void ReadLocalOutOfBandPairingData(
      const BluetoothOutOfBandPairingDataCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE;

 private:
  friend class BluetoothAdapterFactory;
  friend class BluetoothDeviceChromeOs;
  friend class MockBluetoothAdapter;

  BluetoothAdapterChromeOs();
  virtual ~BluetoothAdapterChromeOs();

  // Obtains the default adapter object path from the Bluetooth Daemon
  // and tracks future changes to it.
  void TrackDefaultAdapter();

  // Obtains the object paht for the adapter named by |address| from the
  // Bluetooth Daemon.
  void FindAdapter(const std::string& address);

  // Called by dbus:: in response to the method call sent by both
  // DefaultAdapter() and FindAdapter(), |object_path| will contain the
  // dbus object path of the requested adapter when |success| is true.
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

  // Called by dbus:: in response to the method calls send by SetDiscovering().
  // |callback| and |error_callback| are the callbacks passed to
  // SetDiscovering().
  void OnStartDiscovery(const base::Closure& callback,
                        const ErrorCallback& error_callback,
                        const dbus::ObjectPath& adapter_path,
                        bool success);
  void OnStopDiscovery(const base::Closure& callback,
                       const ErrorCallback& error_callback,
                       const dbus::ObjectPath& adapter_path,
                       bool success);

  // Updates the tracked state of the adapter's discovering state to
  // |discovering| and notifies observers. Called on receipt of a property
  // changed signal, and directly using values obtained from properties.
  void DiscoveringChanged(bool discovering);

  // Called by dbus:: in response to the ReadLocalData method call.
  void OnReadLocalData(const BluetoothOutOfBandPairingDataCallback& callback,
                       const ErrorCallback& error_callback,
                       const BluetoothOutOfBandPairingData& data,
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
  ObserverList<BluetoothAdapter::Observer> observers_;

  // Object path of adapter for this instance, this is fixed at creation time
  // unless |track_default_| is true in which case we update it to always
  // point at the default adapter.
  bool track_default_;
  dbus::ObjectPath object_path_;

  // Tracked adapter state, cached locally so we only send change notifications
  // to observers on a genuine change.
  bool powered_;
  bool discovering_;

  // Devices paired with, connected to, discovered by, or visible to the
  // adapter. The key is the Bluetooth address of the device and the value
  // is the BluetoothDeviceChromeOs object whose lifetime is managed by the
  // adapter instance.
  typedef std::map<const std::string, BluetoothDeviceChromeOs*> DevicesMap;
  DevicesMap devices_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdapterChromeOs> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterChromeOs);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_ADAPTER_CHROMEOS_H_
