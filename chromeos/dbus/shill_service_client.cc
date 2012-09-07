// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill_service_client.h"

#include "base/bind.h"
#include "base/chromeos/chromeos_version.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// Error callback for GetProperties.
void OnGetPropertiesError(
    const dbus::ObjectPath& service_path,
    const ShillServiceClient::DictionaryValueCallback& callback,
    const std::string& error_name,
    const std::string& error_message) {
  const std::string log_string =
      "Failed to call org.chromium.shill.Service.GetProperties for: " +
      service_path.value() + ": " + error_name + ": " + error_message;

  // Suppress ERROR log if error name is
  // "org.freedesktop.DBus.Error.UnknownMethod". crbug.com/130660
  if (error_name == DBUS_ERROR_UNKNOWN_METHOD)
    VLOG(1) << log_string;
  else
    LOG(ERROR) << log_string;

  base::DictionaryValue empty_dictionary;
  callback.Run(DBUS_METHOD_CALL_FAILURE, empty_dictionary);
}

// The ShillServiceClient implementation.
class ShillServiceClientImpl : public ShillServiceClient {
 public:
  explicit ShillServiceClientImpl(dbus::Bus* bus)
      : bus_(bus),
        helpers_deleter_(&helpers_) {
  }

  // ShillServiceClient override.
  virtual void SetPropertyChangedHandler(
      const dbus::ObjectPath& service_path,
      const PropertyChangedHandler& handler) OVERRIDE {
    GetHelper(service_path)->SetPropertyChangedHandler(handler);
  }

  // ShillServiceClient override.
  virtual void ResetPropertyChangedHandler(
      const dbus::ObjectPath& service_path) OVERRIDE {
    GetHelper(service_path)->ResetPropertyChangedHandler();
  }

  // ShillServiceClient override.
  virtual void GetProperties(const dbus::ObjectPath& service_path,
                             const DictionaryValueCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamServiceInterface,
                                 flimflam::kGetPropertiesFunction);
    GetHelper(service_path)->CallDictionaryValueMethodWithErrorCallback(
        &method_call,
        base::Bind(callback, DBUS_METHOD_CALL_SUCCESS),
        base::Bind(&OnGetPropertiesError, service_path, callback));
  }

  // ShillServiceClient override.
  virtual void SetProperty(const dbus::ObjectPath& service_path,
                           const std::string& name,
                           const base::Value& value,
                           const VoidDBusMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamServiceInterface,
                                 flimflam::kSetPropertyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    ShillClientHelper::AppendValueDataAsVariant(&writer, value);
    GetHelper(service_path)->CallVoidMethod(&method_call, callback);
  }

  // ShillServiceClient override.
  virtual void ClearProperty(const dbus::ObjectPath& service_path,
                             const std::string& name,
                             const VoidDBusMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamServiceInterface,
                                 flimflam::kClearPropertyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    GetHelper(service_path)->CallVoidMethod(&method_call, callback);
  }

  // ShillServiceClient override.
  virtual void Connect(const dbus::ObjectPath& service_path,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamServiceInterface,
                                 flimflam::kConnectFunction);
    GetHelper(service_path)->CallVoidMethodWithErrorCallback(
        &method_call, callback, error_callback);
  }

  // ShillServiceClient override.
  virtual void Disconnect(const dbus::ObjectPath& service_path,
                          const VoidDBusMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamServiceInterface,
                                 flimflam::kDisconnectFunction);
    GetHelper(service_path)->CallVoidMethod(&method_call, callback);
  }

  // ShillServiceClient override.
  virtual void Remove(const dbus::ObjectPath& service_path,
                      const VoidDBusMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamServiceInterface,
                                 flimflam::kRemoveServiceFunction);
    GetHelper(service_path)->CallVoidMethod(&method_call, callback);
  }

  // ShillServiceClient override.
  virtual void ActivateCellularModem(
      const dbus::ObjectPath& service_path,
      const std::string& carrier,
      const VoidDBusMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamServiceInterface,
                                 flimflam::kActivateCellularModemFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(carrier);
    GetHelper(service_path)->CallVoidMethod(&method_call, callback);
  }

  // ShillServiceClient override.
  virtual bool CallActivateCellularModemAndBlock(
      const dbus::ObjectPath& service_path,
      const std::string& carrier) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamServiceInterface,
                                 flimflam::kActivateCellularModemFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(carrier);
    return GetHelper(service_path)->CallVoidMethodAndBlock(&method_call);
  }

 private:
  typedef std::map<std::string, ShillClientHelper*> HelperMap;

  // Returns the corresponding ShillClientHelper for the profile.
  ShillClientHelper* GetHelper(const dbus::ObjectPath& service_path) {
    HelperMap::iterator it = helpers_.find(service_path.value());
    if (it != helpers_.end())
      return it->second;

    // There is no helper for the profile, create it.
    dbus::ObjectProxy* object_proxy =
        bus_->GetObjectProxy(flimflam::kFlimflamServiceName, service_path);
    ShillClientHelper* helper = new ShillClientHelper(bus_, object_proxy);
    helper->MonitorPropertyChanged(flimflam::kFlimflamServiceInterface);
    helpers_.insert(HelperMap::value_type(service_path.value(), helper));
    return helper;
  }

  dbus::Bus* bus_;
  HelperMap helpers_;
  STLValueDeleter<HelperMap> helpers_deleter_;

  DISALLOW_COPY_AND_ASSIGN(ShillServiceClientImpl);
};

// A stub implementation of ShillServiceClient.
class ShillServiceClientStubImpl : public ShillServiceClient {
 public:
  ShillServiceClientStubImpl() : weak_ptr_factory_(this) {}

  virtual ~ShillServiceClientStubImpl() {}

  // ShillServiceClient override.
  virtual void SetPropertyChangedHandler(
      const dbus::ObjectPath& service_path,
      const PropertyChangedHandler& handler) OVERRIDE {}

  // ShillServiceClient override.
  virtual void ResetPropertyChangedHandler(
      const dbus::ObjectPath& service_path) OVERRIDE {}

  // ShillServiceClient override.
  virtual void GetProperties(const dbus::ObjectPath& service_path,
                             const DictionaryValueCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ShillServiceClientStubImpl::PassEmptyDictionaryValue,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

  // ShillServiceClient override.
  virtual void SetProperty(const dbus::ObjectPath& service_path,
                           const std::string& name,
                           const base::Value& value,
                           const VoidDBusMethodCallback& callback) OVERRIDE {
    PostSuccessVoidCallback(callback);
  }

  // ShillServiceClient override.
  virtual void ClearProperty(const dbus::ObjectPath& service_path,
                             const std::string& name,
                             const VoidDBusMethodCallback& callback) OVERRIDE {
    PostSuccessVoidCallback(callback);
  }

  // ShillServiceClient override.
  virtual void Connect(const dbus::ObjectPath& service_path,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) OVERRIDE {
    MessageLoop::current()->PostTask(FROM_HERE, callback);
  }

  // ShillServiceClient override.
  virtual void Disconnect(const dbus::ObjectPath& service_path,
                          const VoidDBusMethodCallback& callback) OVERRIDE {
    PostSuccessVoidCallback(callback);
  }

  // ShillServiceClient override.
  virtual void Remove(const dbus::ObjectPath& service_path,
                      const VoidDBusMethodCallback& callback) OVERRIDE {
    PostSuccessVoidCallback(callback);
  }

  // ShillServiceClient override.
  virtual void ActivateCellularModem(
      const dbus::ObjectPath& service_path,
      const std::string& carrier,
      const VoidDBusMethodCallback& callback) OVERRIDE {
    PostSuccessVoidCallback(callback);
  }

  // ShillServiceClient override.
  virtual bool CallActivateCellularModemAndBlock(
      const dbus::ObjectPath& service_path,
      const std::string& carrier) OVERRIDE {
    return true;
  }

 private:
  void PassEmptyDictionaryValue(const DictionaryValueCallback& callback) const {
    base::DictionaryValue dictionary;
    callback.Run(DBUS_METHOD_CALL_SUCCESS, dictionary);
  }

  // Posts a task to run a void callback with success status code.
  void PostSuccessVoidCallback(const VoidDBusMethodCallback& callback) {
    MessageLoop::current()->PostTask(FROM_HERE,
                                     base::Bind(callback,
                                                DBUS_METHOD_CALL_SUCCESS));
  }

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ShillServiceClientStubImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShillServiceClientStubImpl);
};

}  // namespace

ShillServiceClient::ShillServiceClient() {}

ShillServiceClient::~ShillServiceClient() {}

// static
ShillServiceClient* ShillServiceClient::Create(
    DBusClientImplementationType type,
    dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new ShillServiceClientImpl(bus);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new ShillServiceClientStubImpl();
}

}  // namespace chromeos
