// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill_device_client.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chromeos/dbus/shill_device_client_stub.h"
#include "chromeos/dbus/shill_property_changed_observer.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// The ShillDeviceClient implementation.
class ShillDeviceClientImpl : public ShillDeviceClient {
 public:
  explicit ShillDeviceClientImpl(dbus::Bus* bus)
      : bus_(bus) {
  }

  virtual ~ShillDeviceClientImpl() {
    for (HelperMap::iterator iter = helpers_.begin();
         iter != helpers_.end(); ++iter) {
      // This *should* never happen, yet we're getting crash reports that
      // seem to imply that it does happen sometimes.  Adding CHECKs here
      // so we can determine more accurately where the problem lies.
      // See: http://crbug.com/170541
      CHECK(iter->second) << "NULL Helper found in helper list.";
      delete iter->second;
    }
    helpers_.clear();
  }

  ///////////////////////////////////////
  // ShillDeviceClient overrides.
  virtual void AddPropertyChangedObserver(
      const dbus::ObjectPath& device_path,
      ShillPropertyChangedObserver* observer) OVERRIDE {
    GetHelper(device_path)->AddPropertyChangedObserver(observer);
  }

  virtual void RemovePropertyChangedObserver(
      const dbus::ObjectPath& device_path,
      ShillPropertyChangedObserver* observer) OVERRIDE {
    GetHelper(device_path)->RemovePropertyChangedObserver(observer);
  }

  virtual void GetProperties(const dbus::ObjectPath& device_path,
                             const DictionaryValueCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamDeviceInterface,
                                 flimflam::kGetPropertiesFunction);
    GetHelper(device_path)->CallDictionaryValueMethod(&method_call, callback);
  }

  virtual base::DictionaryValue* CallGetPropertiesAndBlock(
      const dbus::ObjectPath& device_path) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamDeviceInterface,
                                 flimflam::kGetPropertiesFunction);
    return GetHelper(device_path)->CallDictionaryValueMethodAndBlock(
        &method_call);
  }

  virtual void ProposeScan(const dbus::ObjectPath& device_path,
                           const VoidDBusMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamDeviceInterface,
                                 flimflam::kProposeScanFunction);
    GetHelper(device_path)->CallVoidMethod(&method_call, callback);
  }

  virtual void SetProperty(const dbus::ObjectPath& device_path,
                           const std::string& name,
                           const base::Value& value,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamDeviceInterface,
                                 flimflam::kSetPropertyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    ShillClientHelper::AppendValueDataAsVariant(&writer, value);
    GetHelper(device_path)->CallVoidMethodWithErrorCallback(&method_call,
                                                            callback,
                                                            error_callback);
  }

  virtual void ClearProperty(const dbus::ObjectPath& device_path,
                             const std::string& name,
                             const VoidDBusMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamDeviceInterface,
                                 flimflam::kClearPropertyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    GetHelper(device_path)->CallVoidMethod(&method_call, callback);
  }

  virtual void AddIPConfig(
      const dbus::ObjectPath& device_path,
      const std::string& method,
      const ObjectPathDBusMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamDeviceInterface,
                                 flimflam::kAddIPConfigFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(method);
    GetHelper(device_path)->CallObjectPathMethod(&method_call, callback);
  }

  virtual void RequirePin(const dbus::ObjectPath& device_path,
                          const std::string& pin,
                          bool require,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamDeviceInterface,
                                 flimflam::kRequirePinFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(pin);
    writer.AppendBool(require);
    GetHelper(device_path)->CallVoidMethodWithErrorCallback(
        &method_call, callback, error_callback);
  }

  virtual void EnterPin(const dbus::ObjectPath& device_path,
                        const std::string& pin,
                        const base::Closure& callback,
                        const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamDeviceInterface,
                                 flimflam::kEnterPinFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(pin);
    GetHelper(device_path)->CallVoidMethodWithErrorCallback(
        &method_call, callback, error_callback);
  }

  virtual void UnblockPin(const dbus::ObjectPath& device_path,
                          const std::string& puk,
                          const std::string& pin,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamDeviceInterface,
                                 flimflam::kUnblockPinFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(puk);
    writer.AppendString(pin);
    GetHelper(device_path)->CallVoidMethodWithErrorCallback(
        &method_call, callback, error_callback);
  }

  virtual void ChangePin(const dbus::ObjectPath& device_path,
                         const std::string& old_pin,
                         const std::string& new_pin,
                         const base::Closure& callback,
                         const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamDeviceInterface,
                                 flimflam::kChangePinFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(old_pin);
    writer.AppendString(new_pin);
    GetHelper(device_path)->CallVoidMethodWithErrorCallback(
        &method_call, callback, error_callback);
  }

  virtual void Register(const dbus::ObjectPath& device_path,
                        const std::string& network_id,
                        const base::Closure& callback,
                        const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamDeviceInterface,
                                 flimflam::kRegisterFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(network_id);
    GetHelper(device_path)->CallVoidMethodWithErrorCallback(
        &method_call, callback, error_callback);
  }

  virtual void SetCarrier(const dbus::ObjectPath& device_path,
                          const std::string& carrier,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamDeviceInterface,
                                 shill::kSetCarrierFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(carrier);
    GetHelper(device_path)->CallVoidMethodWithErrorCallback(
        &method_call, callback, error_callback);
  }

  virtual void Reset(const dbus::ObjectPath& device_path,
                     const base::Closure& callback,
                     const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamDeviceInterface,
                                 shill::kResetFunction);
    GetHelper(device_path)->CallVoidMethodWithErrorCallback(
        &method_call, callback, error_callback);
  }

  virtual TestInterface* GetTestInterface() OVERRIDE {
    return NULL;
  }

 private:
  typedef std::map<std::string, ShillClientHelper*> HelperMap;

  // Returns the corresponding ShillClientHelper for the profile.
  ShillClientHelper* GetHelper(const dbus::ObjectPath& device_path) {
    HelperMap::iterator it = helpers_.find(device_path.value());
    if (it != helpers_.end()) {
      CHECK(it->second) << "Found a NULL helper in the list.";
      return it->second;
    }

    // There is no helper for the profile, create it.
    dbus::ObjectProxy* object_proxy =
        bus_->GetObjectProxy(flimflam::kFlimflamServiceName, device_path);
    ShillClientHelper* helper = new ShillClientHelper(bus_, object_proxy);
    CHECK(helper) << "Unable to create Shill client helper.";
    helper->MonitorPropertyChanged(flimflam::kFlimflamDeviceInterface);
    helpers_.insert(HelperMap::value_type(device_path.value(), helper));
    return helper;
  }

  dbus::Bus* bus_;
  HelperMap helpers_;

  DISALLOW_COPY_AND_ASSIGN(ShillDeviceClientImpl);
};

}  // namespace

ShillDeviceClient::ShillDeviceClient() {}

ShillDeviceClient::~ShillDeviceClient() {}

// static
ShillDeviceClient* ShillDeviceClient::Create(
    DBusClientImplementationType type,
    dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new ShillDeviceClientImpl(bus);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new ShillDeviceClientStub();
}

}  // namespace chromeos
