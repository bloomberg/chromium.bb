// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill_device_client.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chromeos/dbus/shill_property_changed_observer.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"
#include "net/base/ip_endpoint.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// The ShillDeviceClient implementation.
class ShillDeviceClientImpl : public ShillDeviceClient {
 public:
  ShillDeviceClientImpl() : bus_(NULL) {}

  ~ShillDeviceClientImpl() override {
    for (HelperMap::iterator iter = helpers_.begin(); iter != helpers_.end();
         ++iter) {
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
  void AddPropertyChangedObserver(
      const dbus::ObjectPath& device_path,
      ShillPropertyChangedObserver* observer) override {
    GetHelper(device_path)->AddPropertyChangedObserver(observer);
  }

  void RemovePropertyChangedObserver(
      const dbus::ObjectPath& device_path,
      ShillPropertyChangedObserver* observer) override {
    GetHelper(device_path)->RemovePropertyChangedObserver(observer);
  }

  void GetProperties(const dbus::ObjectPath& device_path,
                     const DictionaryValueCallback& callback) override {
    dbus::MethodCall method_call(shill::kFlimflamDeviceInterface,
                                 shill::kGetPropertiesFunction);
    GetHelper(device_path)->CallDictionaryValueMethod(&method_call, callback);
  }

  void SetProperty(const dbus::ObjectPath& device_path,
                   const std::string& name,
                   const base::Value& value,
                   const base::Closure& callback,
                   const ErrorCallback& error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamDeviceInterface,
                                 shill::kSetPropertyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    ShillClientHelper::AppendValueDataAsVariant(&writer, value);
    GetHelper(device_path)
        ->CallVoidMethodWithErrorCallback(&method_call, callback,
                                          error_callback);
  }

  void ClearProperty(const dbus::ObjectPath& device_path,
                     const std::string& name,
                     VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(shill::kFlimflamDeviceInterface,
                                 shill::kClearPropertyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    GetHelper(device_path)->CallVoidMethod(&method_call, std::move(callback));
  }

  void RequirePin(const dbus::ObjectPath& device_path,
                  const std::string& pin,
                  bool require,
                  const base::Closure& callback,
                  const ErrorCallback& error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamDeviceInterface,
                                 shill::kRequirePinFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(pin);
    writer.AppendBool(require);
    GetHelper(device_path)
        ->CallVoidMethodWithErrorCallback(&method_call, callback,
                                          error_callback);
  }

  void EnterPin(const dbus::ObjectPath& device_path,
                const std::string& pin,
                const base::Closure& callback,
                const ErrorCallback& error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamDeviceInterface,
                                 shill::kEnterPinFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(pin);
    GetHelper(device_path)
        ->CallVoidMethodWithErrorCallback(&method_call, callback,
                                          error_callback);
  }

  void UnblockPin(const dbus::ObjectPath& device_path,
                  const std::string& puk,
                  const std::string& pin,
                  const base::Closure& callback,
                  const ErrorCallback& error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamDeviceInterface,
                                 shill::kUnblockPinFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(puk);
    writer.AppendString(pin);
    GetHelper(device_path)
        ->CallVoidMethodWithErrorCallback(&method_call, callback,
                                          error_callback);
  }

  void ChangePin(const dbus::ObjectPath& device_path,
                 const std::string& old_pin,
                 const std::string& new_pin,
                 const base::Closure& callback,
                 const ErrorCallback& error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamDeviceInterface,
                                 shill::kChangePinFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(old_pin);
    writer.AppendString(new_pin);
    GetHelper(device_path)
        ->CallVoidMethodWithErrorCallback(&method_call, callback,
                                          error_callback);
  }

  void Register(const dbus::ObjectPath& device_path,
                const std::string& network_id,
                const base::Closure& callback,
                const ErrorCallback& error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamDeviceInterface,
                                 shill::kRegisterFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(network_id);
    GetHelper(device_path)
        ->CallVoidMethodWithErrorCallback(&method_call, callback,
                                          error_callback);
  }

  void SetCarrier(const dbus::ObjectPath& device_path,
                  const std::string& carrier,
                  const base::Closure& callback,
                  const ErrorCallback& error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamDeviceInterface,
                                 shill::kSetCarrierFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(carrier);
    GetHelper(device_path)
        ->CallVoidMethodWithErrorCallback(&method_call, callback,
                                          error_callback);
  }

  void Reset(const dbus::ObjectPath& device_path,
             const base::Closure& callback,
             const ErrorCallback& error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamDeviceInterface,
                                 shill::kResetFunction);
    GetHelper(device_path)
        ->CallVoidMethodWithErrorCallback(&method_call, callback,
                                          error_callback);
  }

  void PerformTDLSOperation(const dbus::ObjectPath& device_path,
                            const std::string& operation,
                            const std::string& peer,
                            const StringCallback& callback,
                            const ErrorCallback& error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamDeviceInterface,
                                 shill::kPerformTDLSOperationFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(operation);
    writer.AppendString(peer);
    GetHelper(device_path)
        ->CallStringMethodWithErrorCallback(&method_call, callback,
                                            error_callback);
  }

  void AddWakeOnPacketConnection(const dbus::ObjectPath& device_path,
                                 const net::IPEndPoint& ip_endpoint,
                                 const base::Closure& callback,
                                 const ErrorCallback& error_callback) override {
    if (ip_endpoint.address().empty()) {
      LOG(ERROR) << "AddWakeOnPacketConnection: null address";
      return;
    }
    dbus::MethodCall method_call(shill::kFlimflamDeviceInterface,
                                 shill::kAddWakeOnPacketConnectionFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(ip_endpoint.ToStringWithoutPort());
    GetHelper(device_path)
        ->CallVoidMethodWithErrorCallback(&method_call, callback,
                                          error_callback);
  }

  void RemoveWakeOnPacketConnection(
      const dbus::ObjectPath& device_path,
      const net::IPEndPoint& ip_endpoint,
      const base::Closure& callback,
      const ErrorCallback& error_callback) override {
    if (ip_endpoint.address().empty()) {
      LOG(ERROR) << "RemoveWakeOnPacketConnection: null address";
      return;
    }
    dbus::MethodCall method_call(shill::kFlimflamDeviceInterface,
                                 shill::kRemoveWakeOnPacketConnectionFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(ip_endpoint.ToStringWithoutPort());
    GetHelper(device_path)
        ->CallVoidMethodWithErrorCallback(&method_call, callback,
                                          error_callback);
  }

  void RemoveAllWakeOnPacketConnections(
      const dbus::ObjectPath& device_path,
      const base::Closure& callback,
      const ErrorCallback& error_callback) override {
    dbus::MethodCall method_call(
        shill::kFlimflamDeviceInterface,
        shill::kRemoveAllWakeOnPacketConnectionsFunction);
    GetHelper(device_path)
        ->CallVoidMethodWithErrorCallback(&method_call, callback,
                                          error_callback);
  }

  TestInterface* GetTestInterface() override { return NULL; }

 protected:
  void Init(dbus::Bus* bus) override { bus_ = bus; }

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
        bus_->GetObjectProxy(shill::kFlimflamServiceName, device_path);
    ShillClientHelper* helper = new ShillClientHelper(object_proxy);
    CHECK(helper) << "Unable to create Shill client helper.";
    helper->MonitorPropertyChanged(shill::kFlimflamDeviceInterface);
    helpers_.insert(HelperMap::value_type(device_path.value(), helper));
    return helper;
  }

  dbus::Bus* bus_;
  HelperMap helpers_;

  DISALLOW_COPY_AND_ASSIGN(ShillDeviceClientImpl);
};

}  // namespace

ShillDeviceClient::ShillDeviceClient() = default;

ShillDeviceClient::~ShillDeviceClient() = default;

// static
ShillDeviceClient* ShillDeviceClient::Create() {
  return new ShillDeviceClientImpl();
}

}  // namespace chromeos
