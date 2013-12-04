// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill_service_client.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chromeos/dbus/shill_property_changed_observer.h"
#include "chromeos/network/network_event_log.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

#ifndef DBUS_ERROR_UNKNOWN_OBJECT
// The linux_chromeos ASAN builder has an older version of dbus-protocol.h
// so make sure this is defined.
#define DBUS_ERROR_UNKNOWN_OBJECT "org.freedesktop.DBus.Error.UnknownObject"
#endif

// Error callback for GetProperties.
void OnGetDictionaryError(
    const std::string& method_name,
    const dbus::ObjectPath& service_path,
    const ShillServiceClient::DictionaryValueCallback& callback,
    const std::string& error_name,
    const std::string& error_message) {
  const std::string log_string =
      "Failed to call org.chromium.shill.Service." + method_name +
      " for: " + service_path.value() + ": " +
      error_name + ": " + error_message;

  // Suppress ERROR messages for UnknownMethod/Object" since this can
  // happen under normal conditions. See crbug.com/130660 and crbug.com/222210.
  if (error_name == DBUS_ERROR_UNKNOWN_METHOD ||
      error_name == DBUS_ERROR_UNKNOWN_OBJECT)
    VLOG(1) << log_string;
  else
    LOG(ERROR) << log_string;

  base::DictionaryValue empty_dictionary;
  callback.Run(DBUS_METHOD_CALL_FAILURE, empty_dictionary);
}

// The ShillServiceClient implementation.
class ShillServiceClientImpl : public ShillServiceClient {
 public:
  explicit ShillServiceClientImpl()
      : bus_(NULL),
        weak_ptr_factory_(this) {
  }

  virtual ~ShillServiceClientImpl() {
    for (HelperMap::iterator iter = helpers_.begin();
         iter != helpers_.end(); ++iter) {
      ShillClientHelper* helper = iter->second;
      bus_->RemoveObjectProxy(shill::kFlimflamServiceName,
                              helper->object_proxy()->object_path(),
                              base::Bind(&base::DoNothing));
      delete helper;
    }
  }

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
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kGetPropertiesFunction);
    GetHelper(service_path)->CallDictionaryValueMethodWithErrorCallback(
        &method_call,
        base::Bind(callback, DBUS_METHOD_CALL_SUCCESS),
        base::Bind(&OnGetDictionaryError, "GetProperties",
                   service_path, callback));
  }

  virtual void SetProperty(const dbus::ObjectPath& service_path,
                           const std::string& name,
                           const base::Value& value,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kSetPropertyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    ShillClientHelper::AppendValueDataAsVariant(&writer, value);
    GetHelper(service_path)->CallVoidMethodWithErrorCallback(&method_call,
                                                             callback,
                                                             error_callback);
  }

  virtual void SetProperties(const dbus::ObjectPath& service_path,
                             const base::DictionaryValue& properties,
                             const base::Closure& callback,
                             const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kSetPropertiesFunction);
    dbus::MessageWriter writer(&method_call);
    ShillClientHelper::AppendServicePropertiesDictionary(&writer, properties);
    GetHelper(service_path)->CallVoidMethodWithErrorCallback(&method_call,
                                                             callback,
                                                             error_callback);
  }

  virtual void ClearProperty(const dbus::ObjectPath& service_path,
                             const std::string& name,
                             const base::Closure& callback,
                             const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kClearPropertyFunction);
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
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
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
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kConnectFunction);
    GetHelper(service_path)->CallVoidMethodWithErrorCallback(
        &method_call, callback, error_callback);
  }

  virtual void Disconnect(const dbus::ObjectPath& service_path,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kDisconnectFunction);
    GetHelper(service_path)->CallVoidMethodWithErrorCallback(&method_call,
                                                             callback,
                                                             error_callback);
  }

  virtual void Remove(const dbus::ObjectPath& service_path,
                      const base::Closure& callback,
                      const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kRemoveServiceFunction);
    GetHelper(service_path)->CallVoidMethodWithErrorCallback(&method_call,
                                                             callback,
                                                             error_callback);
  }

  virtual void ActivateCellularModem(
      const dbus::ObjectPath& service_path,
      const std::string& carrier,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kActivateCellularModemFunction);
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
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kCompleteCellularActivationFunction);
    dbus::MessageWriter writer(&method_call);
    GetHelper(service_path)->CallVoidMethodWithErrorCallback(&method_call,
                                                             callback,
                                                             error_callback);
  }

  virtual void GetLoadableProfileEntries(
      const dbus::ObjectPath& service_path,
      const DictionaryValueCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kGetLoadableProfileEntriesFunction);
    GetHelper(service_path)->CallDictionaryValueMethodWithErrorCallback(
        &method_call,
        base::Bind(callback, DBUS_METHOD_CALL_SUCCESS),
        base::Bind(&OnGetDictionaryError, "GetLoadableProfileEntries",
                   service_path, callback));
  }

  virtual ShillServiceClient::TestInterface* GetTestInterface() OVERRIDE {
    return NULL;
  }

 protected:
  virtual void Init(dbus::Bus* bus) OVERRIDE {
    bus_ = bus;
  }

 private:
  typedef std::map<std::string, ShillClientHelper*> HelperMap;

  // Returns the corresponding ShillClientHelper for the profile.
  ShillClientHelper* GetHelper(const dbus::ObjectPath& service_path) {
    HelperMap::iterator it = helpers_.find(service_path.value());
    if (it != helpers_.end())
      return it->second;

    // There is no helper for the profile, create it.
    NET_LOG_DEBUG("AddShillClientHelper", service_path.value());
    dbus::ObjectProxy* object_proxy =
        bus_->GetObjectProxy(shill::kFlimflamServiceName, service_path);
    ShillClientHelper* helper = new ShillClientHelper(object_proxy);
    helper->SetReleasedCallback(
        base::Bind(&ShillServiceClientImpl::NotifyReleased,
                   weak_ptr_factory_.GetWeakPtr()));
    helper->MonitorPropertyChanged(shill::kFlimflamServiceInterface);
    helpers_.insert(HelperMap::value_type(service_path.value(), helper));
    return helper;
  }

  void NotifyReleased(ShillClientHelper* helper) {
    // New Shill Service DBus objects are created relatively frequently, so
    // remove them when they become inactive (no observers and no active method
    // calls).
    dbus::ObjectPath object_path = helper->object_proxy()->object_path();
    // Make sure we don't release the proxy used by ShillManagerClient ("/").
    // This shouldn't ever happen, but might if a bug in the code requests
    // a service with path "/", or a bug in Shill passes "/" as a service path.
    // Either way this would cause an invalid memory access in
    // ShillManagerClient, see crbug.com/324849.
    if (object_path == dbus::ObjectPath(shill::kFlimflamServicePath)) {
      NET_LOG_ERROR("ShillServiceClient service has invalid path",
                    shill::kFlimflamServicePath);
      return;
    }
    NET_LOG_DEBUG("RemoveShillClientHelper", object_path.value());
    bus_->RemoveObjectProxy(shill::kFlimflamServiceName,
                            object_path, base::Bind(&base::DoNothing));
    helpers_.erase(object_path.value());
    delete helper;
  }

  dbus::Bus* bus_;
  HelperMap helpers_;
  base::WeakPtrFactory<ShillServiceClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShillServiceClientImpl);
};

}  // namespace

ShillServiceClient::ShillServiceClient() {}

ShillServiceClient::~ShillServiceClient() {}

// static
ShillServiceClient* ShillServiceClient::Create() {
  return new ShillServiceClientImpl();
}

}  // namespace chromeos
