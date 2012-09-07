// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/bluetooth_out_of_band_client.h"

#include <map>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "chromeos/dbus/bluetooth_adapter_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// The BluetoothOutOfBandClient implementation used in production.
class BluetoothOutOfBandClientImpl: public BluetoothOutOfBandClient {
 public:
  explicit BluetoothOutOfBandClientImpl(dbus::Bus* bus)
      : bus_(bus),
        weak_ptr_factory_(this) {}

  virtual ~BluetoothOutOfBandClientImpl() {}

  // BluetoothOutOfBandClient override.
  virtual void ReadLocalData(
      const dbus::ObjectPath& object_path,
      const DataCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_outofband::kBluetoothOutOfBandInterface,
        bluetooth_outofband::kReadLocalData);

    dbus::ObjectProxy* object_proxy = GetObjectProxy(object_path);

    object_proxy->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&BluetoothOutOfBandClientImpl::OnReadLocalData,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

  // BluetoothOutOfBandClient override.
  virtual void AddRemoteData(
      const dbus::ObjectPath& object_path,
      const std::string& address,
      const BluetoothOutOfBandPairingData& data,
      const SuccessCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_outofband::kBluetoothOutOfBandInterface,
        bluetooth_outofband::kAddRemoteData);

    dbus::MessageWriter writer(&method_call);
    writer.AppendString(address);
    writer.AppendArrayOfBytes(data.hash, kBluetoothOutOfBandPairingDataSize);
    writer.AppendArrayOfBytes(data.randomizer,
        kBluetoothOutOfBandPairingDataSize);

    dbus::ObjectProxy* object_proxy = GetObjectProxy(object_path);

    object_proxy->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&BluetoothOutOfBandClientImpl::ResponseToSuccessCallback,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

  // BluetoothOutOfBandClient override.
  virtual void RemoveRemoteData(
      const dbus::ObjectPath& object_path,
      const std::string& address,
      const SuccessCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_outofband::kBluetoothOutOfBandInterface,
        bluetooth_outofband::kRemoveRemoteData);

    dbus::MessageWriter writer(&method_call);
    writer.AppendString(address);

    dbus::ObjectProxy* object_proxy = GetObjectProxy(object_path);

    object_proxy->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&BluetoothOutOfBandClientImpl::ResponseToSuccessCallback,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

 private:
  // We maintain a collection of dbus object proxies for each binding.
  typedef std::map<const dbus::ObjectPath, dbus::ObjectProxy*> ObjectMap;
  ObjectMap object_map_;

  // Returns a pointer to the object proxy for |object_path|, creating
  // it if necessary.  This is cached in the ObjectMap.
  dbus::ObjectProxy* GetObjectProxy(const dbus::ObjectPath& object_path) {
    ObjectMap::iterator iter = object_map_.find(object_path);
    if (iter != object_map_.end())
      return iter->second;

    DCHECK(bus_);
    dbus::ObjectProxy* object_proxy = bus_->GetObjectProxy(
        bluetooth_outofband::kBluetoothOutOfBandServiceName, object_path);

    object_map_[object_path] = object_proxy;
    return object_proxy;
  }

  // Called when a response from ReadLocalOutOfBandPairingData() is received.
  void OnReadLocalData(const DataCallback& callback,
                       dbus::Response* response) {
    bool success = false;
    BluetoothOutOfBandPairingData data;
    if (response != NULL) {
      dbus::MessageReader reader(response);
      uint8_t* bytes = NULL;
      size_t length = kBluetoothOutOfBandPairingDataSize;
      if (reader.PopArrayOfBytes(&bytes, &length)) {
        if (length == kBluetoothOutOfBandPairingDataSize) {
          memcpy(&data.hash, bytes, length);
          if (reader.PopArrayOfBytes(&bytes, &length)) {
            if (length == kBluetoothOutOfBandPairingDataSize) {
              memcpy(&data.randomizer, bytes, length);
              success = true;
            }
          }
        }
      }
    }
    callback.Run(data, success);
  }

  // Translates a dbus::Response to a SuccessCallback by assuming success if
  // |response| is not NULL.
  void ResponseToSuccessCallback(const SuccessCallback& callback,
                                 dbus::Response* response) {
    callback.Run(response != NULL);
  }

  dbus::Bus* bus_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothOutOfBandClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothOutOfBandClientImpl);
};

// The BluetoothOutOfBandClient implementation used on Linux desktop, which does
// nothing.
class BluetoothOutOfBandClientStubImpl : public BluetoothOutOfBandClient {
 public:
  // BluetoothOutOfBandClient override.
  virtual void ReadLocalData(
      const dbus::ObjectPath& object_path,
      const DataCallback& callback) OVERRIDE {
    VLOG(1) << "ReadLocalData: " << object_path.value();
    BluetoothOutOfBandPairingData data;
    callback.Run(data, false);
  }

  // BluetoothOutOfBandClient override.
  virtual void AddRemoteData(
      const dbus::ObjectPath& object_path,
      const std::string& address,
      const BluetoothOutOfBandPairingData& data,
      const SuccessCallback& callback) OVERRIDE {
    VLOG(1) << "AddRemoteData: " << object_path.value();
    callback.Run(false);
  }

  // BluetoothOutOfBandClient override.
  virtual void RemoveRemoteData(
      const dbus::ObjectPath& object_path,
      const std::string& address,
      const SuccessCallback& callback) OVERRIDE {
    VLOG(1) << "RemoveRemoteData: " << object_path.value();
    callback.Run(false);
  }
};

BluetoothOutOfBandClient::BluetoothOutOfBandClient() {}

BluetoothOutOfBandClient::~BluetoothOutOfBandClient() {}

BluetoothOutOfBandClient* BluetoothOutOfBandClient::Create(
    DBusClientImplementationType type,
    dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new BluetoothOutOfBandClientImpl(bus);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new BluetoothOutOfBandClientStubImpl();
}

}  // namespace chromeos
