// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill_service_client.h"

#include "base/bind.h"
#include "base/chromeos/chromeos_version.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chromeos/dbus/shill_property_changed_observer.h"
#include "chromeos/dbus/shill_service_client_stub.h"
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

  /////////////////////////////////////
  // ShillServiceClient overrides.
  virtual void AddPropertyChangedObserver(
      const dbus::ObjectPath& service_path,
      ShillPropertyChangedObserver* observer) OVERRIDE {
    GetHelper(service_path)->AddPropertyChangedObserver(observer);
  }

  virtual void RemovePropertyChangedObserver(
      const dbus::ObjectPath& service_path,
      ShillPropertyChangedObserver* observer) OVERRIDE {
    GetHelper(service_path)->RemovePropertyChangedObserver(observer);
  }

  virtual void GetProperties(const dbus::ObjectPath& service_path,
                             const DictionaryValueCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamServiceInterface,
                                 flimflam::kGetPropertiesFunction);
    GetHelper(service_path)->CallDictionaryValueMethodWithErrorCallback(
        &method_call,
        base::Bind(callback, DBUS_METHOD_CALL_SUCCESS),
        base::Bind(&OnGetPropertiesError, service_path, callback));
  }

  virtual void SetProperty(const dbus::ObjectPath& service_path,
                           const std::string& name,
                           const base::Value& value,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamServiceInterface,
                                 flimflam::kSetPropertyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    ShillClientHelper::AppendValueDataAsVariant(&writer, value);
    GetHelper(service_path)->CallVoidMethodWithErrorCallback(&method_call,
                                                             callback,
                                                             error_callback);
  }

  virtual void ClearProperty(const dbus::ObjectPath& service_path,
                             const std::string& name,
                             const base::Closure& callback,
                             const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamServiceInterface,
                                 flimflam::kClearPropertyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    GetHelper(service_path)->CallVoidMethodWithErrorCallback(&method_call,
                                                             callback,
                                                             error_callback);
  }


  virtual void ClearProperties(const dbus::ObjectPath& service_path,
                               const std::vector<std::string>& names,
                               const ListValueCallback& callback,
                               const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamServiceInterface,
                                 shill::kClearPropertiesFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendArrayOfStrings(names);
    GetHelper(service_path)->CallListValueMethodWithErrorCallback(
        &method_call,
        callback,
        error_callback);
  }

  virtual void Connect(const dbus::ObjectPath& service_path,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamServiceInterface,
                                 flimflam::kConnectFunction);
    GetHelper(service_path)->CallVoidMethodWithErrorCallback(
        &method_call, callback, error_callback);
  }

  virtual void Disconnect(const dbus::ObjectPath& service_path,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamServiceInterface,
                                 flimflam::kDisconnectFunction);
    GetHelper(service_path)->CallVoidMethodWithErrorCallback(&method_call,
                                                             callback,
                                                             error_callback);
  }

  virtual void Remove(const dbus::ObjectPath& service_path,
                      const base::Closure& callback,
                      const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamServiceInterface,
                                 flimflam::kRemoveServiceFunction);
    GetHelper(service_path)->CallVoidMethodWithErrorCallback(&method_call,
                                                             callback,
                                                             error_callback);
  }

  virtual void ActivateCellularModem(
      const dbus::ObjectPath& service_path,
      const std::string& carrier,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamServiceInterface,
                                 flimflam::kActivateCellularModemFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(carrier);
    GetHelper(service_path)->CallVoidMethodWithErrorCallback(&method_call,
                                                             callback,
                                                             error_callback);
  }

  virtual void CompleteCellularActivation(
      const dbus::ObjectPath& service_path,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamServiceInterface,
                                 shill::kCompleteCellularActivationFunction);
    dbus::MessageWriter writer(&method_call);
    GetHelper(service_path)->CallVoidMethodWithErrorCallback(&method_call,
                                                             callback,
                                                             error_callback);
  }

  virtual bool CallActivateCellularModemAndBlock(
      const dbus::ObjectPath& service_path,
      const std::string& carrier) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamServiceInterface,
                                 flimflam::kActivateCellularModemFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(carrier);
    return GetHelper(service_path)->CallVoidMethodAndBlock(&method_call);
  }

  virtual ShillServiceClient::TestInterface* GetTestInterface() OVERRIDE {
    return NULL;
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
  return new ShillServiceClientStub();
}

}  // namespace chromeos
