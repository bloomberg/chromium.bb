// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/bluetooth_device_client.h"

#include <map>

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/chromeos/dbus/bluetooth_adapter_client.h"
#include "chrome/browser/chromeos/system/runtime_environment.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// The BluetoothDeviceClient implementation used in production.
class BluetoothDeviceClientImpl: public BluetoothDeviceClient,
                                 private BluetoothAdapterClient::Observer {
 public:
  BluetoothDeviceClientImpl(dbus::Bus* bus,
                            BluetoothAdapterClient* adapter_client)
      : weak_ptr_factory_(this),
        bus_(bus) {
    VLOG(1) << "Creating BluetoothDeviceClientImpl";

    DCHECK(adapter_client);
    adapter_client->AddObserver(this);
  }

  virtual ~BluetoothDeviceClientImpl() {
  }

  // BluetoothDeviceClient override.
  virtual void AddObserver(BluetoothDeviceClient::Observer* observer)
      OVERRIDE {
    VLOG(1) << "AddObserver";
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  // BluetoothDeviceClient override.
  virtual void RemoveObserver(BluetoothDeviceClient::Observer* observer)
      OVERRIDE {
    VLOG(1) << "RemoveObserver";
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

 private:
  // BluetoothAdapterClient::Observer override.
  virtual void DeviceCreated(const std::string& adapter_path,
                             const std::string& object_path) OVERRIDE {
    VLOG(1) << "DeviceCreated: " << object_path;
  }

  // BluetoothAdapterClient::Observer override.
  virtual void DeviceRemoved(const std::string& adapter_path,
                             const std::string& object_path) OVERRIDE {
    VLOG(1) << "DeviceRemoved: " << object_path;
    RemoveObjectProxyForPath(object_path);
  }

  // Ensures that we have a dbus object proxy for a device with dbus
  // object path |object_path|, and if not, creates it stores it in
  // our |proxy_map_| map.
  dbus::ObjectProxy* GetObjectProxyForPath(const std::string& object_path) {
    VLOG(1) << "GetObjectProxyForPath: " << object_path;

    ProxyMap::iterator it = proxy_map_.find(object_path);
    if (it != proxy_map_.end())
      return it->second;

    DCHECK(bus_);
    dbus::ObjectProxy* device_proxy = bus_->GetObjectProxy(
        bluetooth_device::kBluetoothDeviceServiceName, object_path);

    proxy_map_[object_path] = device_proxy;

    return device_proxy;
  }

  // Removes the dbus object proxy for the device with dbus object path
  // |object_path| from our |proxy_map_| map.
  void RemoveObjectProxyForPath(const std::string& object_path) {
    VLOG(1) << "RemoveObjectProxyForPath: " << object_path;
    proxy_map_.erase(object_path);
  }

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  base::WeakPtrFactory<BluetoothDeviceClientImpl> weak_ptr_factory_;

  dbus::Bus* bus_;

  // We maintain a collection of dbus object proxies, one for each device.
  typedef std::map<const std::string, dbus::ObjectProxy*> ProxyMap;
  ProxyMap proxy_map_;

  // List of observers interested in event notifications from us.
  ObserverList<BluetoothDeviceClient::Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDeviceClientImpl);
};

// The BluetoothDeviceClient implementation used on Linux desktop, which does
// nothing.
class BluetoothDeviceClientStubImpl : public BluetoothDeviceClient {
 public:
  // BluetoothDeviceClient override.
  virtual void AddObserver(Observer* observer) OVERRIDE {
    VLOG(1) << "AddObserver";
  }

  // BluetoothDeviceClient override.
  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    VLOG(1) << "RemoveObserver";
  }
};

BluetoothDeviceClient::BluetoothDeviceClient() {
}

BluetoothDeviceClient::~BluetoothDeviceClient() {
}

BluetoothDeviceClient* BluetoothDeviceClient::Create(
    dbus::Bus* bus,
    BluetoothAdapterClient* adapter_client) {
  if (system::runtime_environment::IsRunningOnChromeOS()) {
    return new BluetoothDeviceClientImpl(bus, adapter_client);
  } else {
    return new BluetoothDeviceClientStubImpl();
  }
}

}  // namespace chromeos
