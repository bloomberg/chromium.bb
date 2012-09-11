// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_ADAPTER_H_
#define CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_ADAPTER_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "chromeos/dbus/bluetooth_adapter_client.h"
#include "chromeos/dbus/bluetooth_device_client.h"
#include "chromeos/dbus/bluetooth_manager_client.h"
#include "chromeos/dbus/bluetooth_out_of_band_client.h"
#include "dbus/object_path.h"

namespace chromeos {

class BluetoothDevice;

// The BluetoothAdapter class represents a local Bluetooth adapter which
// may be used to interact with remote Bluetooth devices. As well as
// providing support for determining whether an adapter is present, and
// whether the radio is powered, this class also provides support for
// obtaining the list of remote devices known to the adapter, discovering
// new devices, and providing notification of updates to device information.
//
// The class may be instantiated for either a specific adapter, or for the
// generic "default adapter" which may change depending on availability.
class BluetoothAdapter : public base::RefCounted<BluetoothAdapter>,
                         public BluetoothManagerClient::Observer,
                         public BluetoothAdapterClient::Observer,
                         public BluetoothDeviceClient::Observer {
 public:
  // Interface for observing changes from bluetooth adapters.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the presence of the adapter |adapter| changes, when
    // |present| is true the adapter is now present, false means the adapter
    // has been removed from the system.
    virtual void AdapterPresentChanged(BluetoothAdapter* adapter,
                                       bool present) {}

    // Called when the radio power state of the adapter |adapter| changes,
    // when |powered| is true the adapter radio is powered, false means the
    // adapter radio is off.
    virtual void AdapterPoweredChanged(BluetoothAdapter* adapter,
                                       bool powered) {}

    // Called when the discovering state of the adapter |adapter| changes,
    // when |discovering| is true the adapter is seeking new devices, false
    // means it is not. Note that device discovery involves both states when
    // the adapter is seeking new devices and states when it is not because
    // it is interrogating the devices it found.
    virtual void AdapterDiscoveringChanged(BluetoothAdapter* adapter,
                                           bool discovering) {}

    // Called when a new device |device| is added to the adapter |adapter|,
    // either because it has been discovered or a connection made. |device|
    // should not be cached, instead copy its address.
    virtual void DeviceAdded(BluetoothAdapter* adapter,
                             BluetoothDevice* device) {}

    // Called when properties of the device |device| known to the adapter
    // |adapter| change. |device| should not be cached, instead copy its
    // address.
    virtual void DeviceChanged(BluetoothAdapter* adapter,
                               BluetoothDevice* device) {}

    // Called when the device |device| is removed from the adapter |adapter|,
    // either as a result of a discovered device being lost between discovering
    // phases or pairing information deleted. |device| should not be cached.
    virtual void DeviceRemoved(BluetoothAdapter* adapter,
                               BluetoothDevice* device) {}
  };

  // Adds and removes observers for events on this bluetooth adapter,
  // if monitoring multiple adapters check the |adapter| parameter of
  // observer methods to determine which adapter is issuing the event.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // The ErrorCallback is used for methods that can fail in which case it
  // is called, in the success case the callback is simply not called.
  typedef base::Callback<void()> ErrorCallback;

  // The BluetoothOutOfBandPairingDataCallback is used to return
  // BluetoothOutOfBandPairingData to the caller.
  typedef base::Callback<void(const BluetoothOutOfBandPairingData& data)>
      BluetoothOutOfBandPairingDataCallback;

  // The address of this adapter.  The address format is "XX:XX:XX:XX:XX:XX",
  // where each XX is a hexadecimal number.
  const std::string& address() const { return address_; }

  // The name of the adapter.
  const std::string& name() const { return name_; }

  // Indicates whether the adapter is actually present on the system, for
  // the default adapter this indicates whether any adapter is present. An
  // adapter is only considered present if the address has been obtained.
  virtual bool IsPresent() const;

  // Indicates whether the adapter radio is powered.
  virtual bool IsPowered() const;

  // Requests a change to the adapter radio power, setting |powered| to true
  // will turn on the radio and false will turn it off.  On success, callback
  // will be called.  On failure, |error_callback| will be called.
  void SetPowered(bool powered,
                  const base::Closure& callback,
                  const ErrorCallback& error_callback);

  // Indicates whether the adapter is currently discovering new devices,
  // note that a typical discovery process has phases of this being true
  // followed by phases of being false when the adapter interrogates the
  // devices found.
  virtual bool IsDiscovering() const;

  // Requests that the adapter either begin discovering new devices when
  // |discovering| is true, or cease any discovery when false.  On success,
  // callback will be called.  On failure, |error_callback| will be called.
  virtual void SetDiscovering(bool discovering,
                              const base::Closure& callback,
                              const ErrorCallback& error_callback);

  // Requests the list of devices from the adapter, all are returned
  // including those currently connected and those paired. Use the
  // returned device pointers to determine which they are.
  typedef std::vector<BluetoothDevice*> DeviceList;
  virtual DeviceList GetDevices();
  typedef std::vector<const BluetoothDevice*> ConstDeviceList;
  virtual ConstDeviceList GetDevices() const;

  // Returns a pointer to the device with the given address |address| or
  // NULL if no such device is known.
  virtual BluetoothDevice* GetDevice(const std::string& address);
  virtual const BluetoothDevice* GetDevice(const std::string& address) const;

  // Requests the local Out Of Band pairing data.
  virtual void ReadLocalOutOfBandPairingData(
      const BluetoothOutOfBandPairingDataCallback& callback,
      const ErrorCallback& error_callback);

  // Returns the shared instance for the default adapter, whichever that may
  // be at the time. Use IsPresent() and the AdapterPresentChanged() observer
  // method to determine whether an adapter is actually available or not.
  static scoped_refptr<BluetoothAdapter> DefaultAdapter();

  // Creates an instance for a specific adapter named by |address|, which
  // may be the bluetooth address of the adapter or a device name such as
  // "hci0".
  static BluetoothAdapter* Create(const std::string& address);

 private:
  friend class base::RefCounted<BluetoothAdapter>;
  friend class BluetoothDevice;
  friend class MockBluetoothAdapter;

  BluetoothAdapter();
  virtual ~BluetoothAdapter();

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
        const dbus::ObjectPath& adapter_path, const std::string& address,
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

  // Address of the adapter.
  std::string address_;

  // Name of the adapter.
  std::string name_;

  // Tracked adapter state, cached locally so we only send change notifications
  // to observers on a genuine change.
  bool powered_;
  bool discovering_;

  // Devices paired with, connected to, discovered by, or visible to the
  // adapter. The key is the Bluetooth address of the device and the value
  // is the BluetoothDevice object whose lifetime is managed by the adapter
  // instance.
  typedef std::map<const std::string, BluetoothDevice*> DevicesMap;
  DevicesMap devices_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdapter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapter);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_ADAPTER_H_
