// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill_device_client.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/values.h"
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
      : bus_(bus),
        helpers_deleter_(&helpers_) {
  }

  ///////////////////////////////////////
  // ShillDeviceClient overrides.
  virtual void SetPropertyChangedHandler(
      const dbus::ObjectPath& device_path,
      const PropertyChangedHandler& handler) OVERRIDE {
    GetHelper(device_path)->SetPropertyChangedHandler(handler);
  }

  virtual void ResetPropertyChangedHandler(
      const dbus::ObjectPath& device_path) OVERRIDE {
    GetHelper(device_path)->ResetPropertyChangedHandler();
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
                           const VoidDBusMethodCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamDeviceInterface,
                                 flimflam::kSetPropertyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    ShillClientHelper::AppendValueDataAsVariant(&writer, value);
    GetHelper(device_path)->CallVoidMethod(&method_call, callback);
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

  virtual dbus::ObjectPath CallAddIPConfigAndBlock(
      const dbus::ObjectPath& device_path,
      const std::string& method) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamDeviceInterface,
                                 flimflam::kAddIPConfigFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(method);
    return GetHelper(device_path)->CallObjectPathMethodAndBlock(&method_call);
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

 private:
  typedef std::map<std::string, ShillClientHelper*> HelperMap;

  // Returns the corresponding ShillClientHelper for the profile.
  ShillClientHelper* GetHelper(const dbus::ObjectPath& device_path) {
    HelperMap::iterator it = helpers_.find(device_path.value());
    if (it != helpers_.end())
      return it->second;

    // There is no helper for the profile, create it.
    dbus::ObjectProxy* object_proxy =
        bus_->GetObjectProxy(flimflam::kFlimflamServiceName, device_path);
    ShillClientHelper* helper = new ShillClientHelper(bus_, object_proxy);
    helper->MonitorPropertyChanged(flimflam::kFlimflamDeviceInterface);
    helpers_.insert(HelperMap::value_type(device_path.value(), helper));
    return helper;
  }

  dbus::Bus* bus_;
  HelperMap helpers_;
  STLValueDeleter<HelperMap> helpers_deleter_;

  DISALLOW_COPY_AND_ASSIGN(ShillDeviceClientImpl);
};

// A stub implementation of ShillDeviceClient.
// Implemented: Stub cellular device for SMS testing.
class ShillDeviceClientStubImpl : public ShillDeviceClient {
 public:
  ShillDeviceClientStubImpl() : weak_ptr_factory_(this) {
    // Add a cellular device for SMS. Note: name matches Manager entry.
    const char kStubCellular1[] = "stub_cellular1";
    base::DictionaryValue* cellular_properties = new base::DictionaryValue;
    cellular_properties->SetWithoutPathExpansion(
        flimflam::kTypeProperty,
        base::Value::CreateStringValue(flimflam::kTypeCellular));
    cellular_properties->SetWithoutPathExpansion(
        flimflam::kDBusConnectionProperty,
        base::Value::CreateStringValue("/stub"));
    cellular_properties->SetWithoutPathExpansion(
        flimflam::kDBusObjectProperty,
        base::Value::CreateStringValue("/device/cellular1"));
    stub_devices_.Set(kStubCellular1, cellular_properties);

    // Create a second device stubbing a modem managed by
    // ModemManager1 interfaces.
    // Note: name matches Manager entry.
    const char kStubCellular2[] = "stub_cellular2";
    cellular_properties = new base::DictionaryValue;
    cellular_properties->SetWithoutPathExpansion(
        flimflam::kTypeProperty,
        base::Value::CreateStringValue(flimflam::kTypeCellular));
    cellular_properties->SetWithoutPathExpansion(
        flimflam::kDBusConnectionProperty,
        base::Value::CreateStringValue(":stub.0"));
    cellular_properties->SetWithoutPathExpansion(
        flimflam::kDBusObjectProperty,
        base::Value::CreateStringValue(
                "/org/freedesktop/ModemManager1/stub/0"));
    stub_devices_.Set(kStubCellular2, cellular_properties);
  }

  virtual ~ShillDeviceClientStubImpl() {}

  // ShillDeviceClient override.
  virtual void SetPropertyChangedHandler(
      const dbus::ObjectPath& device_path,
      const PropertyChangedHandler& handler) OVERRIDE {}

  // ShillDeviceClient override.
  virtual void ResetPropertyChangedHandler(
      const dbus::ObjectPath& device_path) OVERRIDE {}

  // ShillDeviceClient override.
  virtual void GetProperties(const dbus::ObjectPath& device_path,
                             const DictionaryValueCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ShillDeviceClientStubImpl::PassStubDevicePrperties,
                   weak_ptr_factory_.GetWeakPtr(),
                   device_path, callback));
  }

  // ShillDeviceClient override.
  virtual base::DictionaryValue* CallGetPropertiesAndBlock(
      const dbus::ObjectPath& device_path) OVERRIDE {
    return new base::DictionaryValue;
  }

  // ShillProfileClient override.
  virtual void ProposeScan(const dbus::ObjectPath& device_path,
                           const VoidDBusMethodCallback& callback) OVERRIDE {
    PostVoidCallback(callback, DBUS_METHOD_CALL_SUCCESS);
  }

  // ShillDeviceClient override.
  virtual void SetProperty(const dbus::ObjectPath& device_path,
                           const std::string& name,
                           const base::Value& value,
                           const VoidDBusMethodCallback& callback) OVERRIDE {
    base::DictionaryValue* device_properties = NULL;
    if (!stub_devices_.GetDictionary(device_path.value(), &device_properties)) {
      PostVoidCallback(callback, DBUS_METHOD_CALL_FAILURE);
      return;
    }
    device_properties->Set(name, value.DeepCopy());
    PostVoidCallback(callback, DBUS_METHOD_CALL_SUCCESS);
  }

  // ShillDeviceClient override.
  virtual void ClearProperty(const dbus::ObjectPath& device_path,
                             const std::string& name,
                             const VoidDBusMethodCallback& callback) OVERRIDE {
    base::DictionaryValue* device_properties = NULL;
    if (!stub_devices_.GetDictionary(device_path.value(), &device_properties)) {
      PostVoidCallback(callback, DBUS_METHOD_CALL_FAILURE);
      return;
    }
    device_properties->Remove(name, NULL);
    PostVoidCallback(callback, DBUS_METHOD_CALL_SUCCESS);
  }

  // ShillDeviceClient override.
  virtual void AddIPConfig(
      const dbus::ObjectPath& device_path,
      const std::string& method,
      const ObjectPathDBusMethodCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(FROM_HERE,
                                     base::Bind(callback,
                                                DBUS_METHOD_CALL_SUCCESS,
                                                dbus::ObjectPath()));
  }

  // ShillDeviceClient override.
  virtual dbus::ObjectPath CallAddIPConfigAndBlock(
      const dbus::ObjectPath& device_path,
      const std::string& method) OVERRIDE {
    return dbus::ObjectPath();
  }

  // ShillDeviceClient override.
  virtual void RequirePin(const dbus::ObjectPath& device_path,
                          const std::string& pin,
                          bool require,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) OVERRIDE {
    MessageLoop::current()->PostTask(FROM_HERE, callback);
  }

  // ShillDeviceClient override.
  virtual void EnterPin(const dbus::ObjectPath& device_path,
                        const std::string& pin,
                        const base::Closure& callback,
                        const ErrorCallback& error_callback) OVERRIDE {
    MessageLoop::current()->PostTask(FROM_HERE, callback);
  }

  // ShillDeviceClient override.
  virtual void UnblockPin(const dbus::ObjectPath& device_path,
                          const std::string& puk,
                          const std::string& pin,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) OVERRIDE {
    MessageLoop::current()->PostTask(FROM_HERE, callback);
  }

  // ShillDeviceClient override.
  virtual void ChangePin(const dbus::ObjectPath& device_path,
                         const std::string& old_pin,
                         const std::string& new_pin,
                         const base::Closure& callback,
                         const ErrorCallback& error_callback) OVERRIDE {
    MessageLoop::current()->PostTask(FROM_HERE, callback);
  }

  // ShillDeviceClient override.
  virtual void Register(const dbus::ObjectPath& device_path,
                        const std::string& network_id,
                        const base::Closure& callback,
                        const ErrorCallback& error_callback) OVERRIDE {
    MessageLoop::current()->PostTask(FROM_HERE, callback);
  }

 private:
  void PassStubDevicePrperties(const dbus::ObjectPath& device_path,
                               const DictionaryValueCallback& callback) const {
    const base::DictionaryValue* device_properties = NULL;
    if (!stub_devices_.GetDictionary(device_path.value(), &device_properties)) {
      base::DictionaryValue empty_dictionary;
      callback.Run(DBUS_METHOD_CALL_FAILURE, empty_dictionary);
      return;
    }
    callback.Run(DBUS_METHOD_CALL_SUCCESS, *device_properties);
  }

  // Posts a task to run a void callback with status code |status|.
  void PostVoidCallback(const VoidDBusMethodCallback& callback,
                        DBusMethodCallStatus status) {
    MessageLoop::current()->PostTask(FROM_HERE,
                                     base::Bind(callback, status));
  }

  // Dictionary of <device_name, Dictionary>.
  base::DictionaryValue stub_devices_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ShillDeviceClientStubImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShillDeviceClientStubImpl);
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
  return new ShillDeviceClientStubImpl();
}

}  // namespace chromeos
