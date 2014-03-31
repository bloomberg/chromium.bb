// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/bluetooth_gatt_manager_client.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

const char BluetoothGattManagerClient::kNoResponseError[] =
    "org.chromium.Error.NoResponse";

// The BluetoothGattManagerClient implementation used in production.
class BluetoothGattManagerClientImpl : public BluetoothGattManagerClient {
 public:
  BluetoothGattManagerClientImpl()
      : object_proxy_(NULL),
        weak_ptr_factory_(this) {
  }

  virtual ~BluetoothGattManagerClientImpl() {
  }

  // BluetoothGattManagerClient override.
  virtual void RegisterService(const dbus::ObjectPath& service_path,
                               const Options& options,
                               const base::Closure& callback,
                               const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_gatt_manager::kBluetoothGattManagerInterface,
        bluetooth_gatt_manager::kRegisterService);

    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(service_path);

    // TODO(armansito): The parameters of the Options dictionary are undefined
    // but the method signature still requires a value dictionary. Pass an
    // empty dictionary and fill in the contents later once this is defined.
    dbus::MessageWriter array_writer(NULL);
    writer.OpenArray("{sv}", &array_writer);
    writer.CloseContainer(&array_writer);

    DCHECK(object_proxy_);
    object_proxy_->CallMethodWithErrorCallback(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&BluetoothGattManagerClientImpl::OnSuccess,
                   weak_ptr_factory_.GetWeakPtr(), callback),
        base::Bind(&BluetoothGattManagerClientImpl::OnError,
                   weak_ptr_factory_.GetWeakPtr(), error_callback));
  }

  // BluetoothGattManagerClient override.
  virtual void UnregisterService(const dbus::ObjectPath& service_path,
                                 const base::Closure& callback,
                                 const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(
        bluetooth_gatt_manager::kBluetoothGattManagerInterface,
        bluetooth_gatt_manager::kUnregisterService);

    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(service_path);

    DCHECK(object_proxy_);
    object_proxy_->CallMethodWithErrorCallback(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&BluetoothGattManagerClientImpl::OnSuccess,
                   weak_ptr_factory_.GetWeakPtr(), callback),
        base::Bind(&BluetoothGattManagerClientImpl::OnError,
                   weak_ptr_factory_.GetWeakPtr(), error_callback));
  }

 protected:
  // chromeos::DBusClient override.
  virtual void Init(dbus::Bus* bus) OVERRIDE {
    DCHECK(bus);
    object_proxy_ = bus->GetObjectProxy(
        bluetooth_gatt_manager::kBluetoothGattManagerServiceName,
        dbus::ObjectPath(
            bluetooth_gatt_manager::kBluetoothGattManagerInterface));
  }

 private:
  // Called when a response for a successful method call is received.
  void OnSuccess(const base::Closure& callback, dbus::Response* response) {
    DCHECK(response);
    callback.Run();
  }

  // Called when a response for a failed method call is received.
  void OnError(const ErrorCallback& error_callback,
               dbus::ErrorResponse* response) {
    // Error response has optional error message argument.
    std::string error_name;
    std::string error_message;
    if (response) {
      dbus::MessageReader reader(response);
      error_name = response->GetErrorName();
      reader.PopString(&error_message);
    } else {
      error_name = kNoResponseError;
    }
    error_callback.Run(error_name, error_message);
  }

  // The proxy to the remote GATT manager object.
  dbus::ObjectProxy* object_proxy_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothGattManagerClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothGattManagerClientImpl);
};

BluetoothGattManagerClient::BluetoothGattManagerClient() {
}

BluetoothGattManagerClient::~BluetoothGattManagerClient() {
}

// static
BluetoothGattManagerClient* BluetoothGattManagerClient::Create() {
  return new BluetoothGattManagerClientImpl();
}

}  // namespace chromeos
