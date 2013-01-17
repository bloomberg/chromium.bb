// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"

namespace device {

class BluetoothDevice;

struct BluetoothOutOfBandPairingData;

// BluetoothAdapter represents a local Bluetooth adapter which may be used to
// interact with remote Bluetooth devices. As well as providing support for
// determining whether an adapter is present, and whether the radio is powered,
// this class also provides support for obtaining the list of remote devices
// known to the adapter, discovering new devices, and providing notification of
// updates to device information.
class BluetoothAdapter : public base::RefCounted<BluetoothAdapter> {
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

  // The ErrorCallback is used for methods that can fail in which case it
  // is called, in the success case the callback is simply not called.
  typedef base::Callback<void()> ErrorCallback;

  // The BluetoothOutOfBandPairingDataCallback is used to return
  // BluetoothOutOfBandPairingData to the caller.
  typedef base::Callback<void(const BluetoothOutOfBandPairingData& data)>
      BluetoothOutOfBandPairingDataCallback;

  typedef base::Callback<void(scoped_refptr<BluetoothAdapter> adapter)>
      AdapterCallback;

  // Adds and removes observers for events on this bluetooth adapter,
  // if monitoring multiple adapters check the |adapter| parameter of
  // observer methods to determine which adapter is issuing the event.
  virtual void AddObserver(BluetoothAdapter::Observer* observer) = 0;
  virtual void RemoveObserver(
      BluetoothAdapter::Observer* observer) = 0;

  // The address of this adapter.  The address format is "XX:XX:XX:XX:XX:XX",
  // where each XX is a hexadecimal number.
  virtual const std::string& address() const;

  // The name of the adapter.
  virtual const std::string& name() const;

  // Queue adapter callbacks to be run when the adapter is initialized.
  virtual void QueueAdapterCallback(const AdapterCallback& callback);

  // Indicates whether the adapter is initialized and ready to use.
  virtual bool IsInitialized() const = 0;

  // Indicates whether the adapter is actually present on the system, for
  // the default adapter this indicates whether any adapter is present. An
  // adapter is only considered present if the address has been obtained.
  virtual bool IsPresent() const = 0;

  // Indicates whether the adapter radio is powered.
  virtual bool IsPowered() const = 0;

  // Requests a change to the adapter radio power, setting |powered| to true
  // will turn on the radio and false will turn it off.  On success, callback
  // will be called.  On failure, |error_callback| will be called.
  virtual void SetPowered(bool powered,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) = 0;

  // Indicates whether the adapter is currently discovering new devices,
  // note that a typical discovery process has phases of this being true
  // followed by phases of being false when the adapter interrogates the
  // devices found.
  virtual bool IsDiscovering() const = 0;

  // Requests that the adapter either begin discovering new devices when
  // |discovering| is true, or cease any discovery when false.  On success,
  // callback will be called.  On failure, |error_callback| will be called.
  virtual void SetDiscovering(bool discovering,
                              const base::Closure& callback,
                              const ErrorCallback& error_callback) = 0;

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
  virtual const BluetoothDevice* GetDevice(
      const std::string& address) const;

  // Requests the local Out Of Band pairing data.
  virtual void ReadLocalOutOfBandPairingData(
      const BluetoothOutOfBandPairingDataCallback& callback,
      const ErrorCallback& error_callback) = 0;

 protected:
  friend class base::RefCounted<BluetoothAdapter>;
  BluetoothAdapter();
  virtual ~BluetoothAdapter();

  // Address of the adapter.
  std::string address_;

  // Name of the adapter.
  std::string name_;

  std::vector<AdapterCallback> adapter_callbacks_;

  // Devices paired with, connected to, discovered by, or visible to the
  // adapter. The key is the Bluetooth address of the device and the value
  // is the BluetoothDevice object whose lifetime is managed by the
  // adapter instance.
  typedef std::map<const std::string, BluetoothDevice*> DevicesMap;
  DevicesMap devices_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_H_
