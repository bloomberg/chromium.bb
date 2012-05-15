// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/flimflam_device_client.h"

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

// The FlimflamDeviceClient implementation.
class FlimflamDeviceClientImpl : public FlimflamDeviceClient {
 public:
  explicit FlimflamDeviceClientImpl(dbus::Bus* bus)
      : bus_(bus),
        helpers_deleter_(&helpers_) {
  }

  // FlimflamProfileClient override.
  virtual void SetPropertyChangedHandler(
      const dbus::ObjectPath& device_path,
      const PropertyChangedHandler& handler) OVERRIDE {
    GetHelper(device_path)->SetPropertyChangedHandler(handler);
  }

  // FlimflamProfileClient override.
  virtual void ResetPropertyChangedHandler(
      const dbus::ObjectPath& device_path) OVERRIDE {
    GetHelper(device_path)->ResetPropertyChangedHandler();
  }

  // FlimflamProfileClient override.
  virtual void GetProperties(const dbus::ObjectPath& device_path,
                             const DictionaryValueCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamDeviceInterface,
                                 flimflam::kGetPropertiesFunction);
    GetHelper(device_path)->CallDictionaryValueMethod(&method_call, callback);
  }

  // FlimflamProfileClient override.
  virtual base::DictionaryValue* CallGetPropertiesAndBlock(
      const dbus::ObjectPath& device_path) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamDeviceInterface,
                                 flimflam::kGetPropertiesFunction);
    return GetHelper(device_path)->CallDictionaryValueMethodAndBlock(
        &method_call);
  }

  // FlimflamProfileClient override.
  virtual void ProposeScan(const dbus::ObjectPath& device_path,
                           const VoidCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamDeviceInterface,
                                 flimflam::kProposeScanFunction);
    GetHelper(device_path)->CallVoidMethod(&method_call, callback);
  }

  // FlimflamProfileClient override.
  virtual void SetProperty(const dbus::ObjectPath& device_path,
                           const std::string& name,
                           const base::Value& value,
                           const VoidCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamDeviceInterface,
                                 flimflam::kSetPropertyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    FlimflamClientHelper::AppendValueDataAsVariant(&writer, value);
    GetHelper(device_path)->CallVoidMethod(&method_call, callback);
  }

  // FlimflamProfileClient override.
  virtual void ClearProperty(const dbus::ObjectPath& device_path,
                             const std::string& name,
                             const VoidCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamDeviceInterface,
                                 flimflam::kClearPropertyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    GetHelper(device_path)->CallVoidMethod(&method_call, callback);
  }

  // FlimflamProfileClient override.
  virtual void AddIPConfig(const dbus::ObjectPath& device_path,
                           const std::string& method,
                           const ObjectPathCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamDeviceInterface,
                                 flimflam::kAddIPConfigFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(method);
    GetHelper(device_path)->CallObjectPathMethod(&method_call, callback);
  }

  // FlimflamProfileClient override.
  virtual dbus::ObjectPath CallAddIPConfigAndBlock(
      const dbus::ObjectPath& device_path,
      const std::string& method) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamDeviceInterface,
                                 flimflam::kAddIPConfigFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(method);
    return GetHelper(device_path)->CallObjectPathMethodAndBlock(&method_call);
  }

  // FlimflamProfileClient override.
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

  // FlimflamProfileClient override.
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

  // FlimflamProfileClient override.
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

  // FlimflamProfileClient override.
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

  // FlimflamProfileClient override.
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
  typedef std::map<std::string, FlimflamClientHelper*> HelperMap;

  // Returns the corresponding FlimflamClientHelper for the profile.
  FlimflamClientHelper* GetHelper(const dbus::ObjectPath& device_path) {
    HelperMap::iterator it = helpers_.find(device_path.value());
    if (it != helpers_.end())
      return it->second;

    // There is no helper for the profile, create it.
    dbus::ObjectProxy* object_proxy =
        bus_->GetObjectProxy(flimflam::kFlimflamServiceName, device_path);
    FlimflamClientHelper* helper = new FlimflamClientHelper(bus_, object_proxy);
    helper->MonitorPropertyChanged(flimflam::kFlimflamDeviceInterface);
    helpers_.insert(HelperMap::value_type(device_path.value(), helper));
    return helper;
  }

  dbus::Bus* bus_;
  HelperMap helpers_;
  STLValueDeleter<HelperMap> helpers_deleter_;

  DISALLOW_COPY_AND_ASSIGN(FlimflamDeviceClientImpl);
};

// A stub implementation of FlimflamDeviceClient.
// Implemented: Stub cellular device for SMS testing.
class FlimflamDeviceClientStubImpl : public FlimflamDeviceClient {
 public:
  FlimflamDeviceClientStubImpl() : weak_ptr_factory_(this) {
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
  }

  virtual ~FlimflamDeviceClientStubImpl() {}

  // FlimflamDeviceClient override.
  virtual void SetPropertyChangedHandler(
      const dbus::ObjectPath& device_path,
      const PropertyChangedHandler& handler) OVERRIDE {}

  // FlimflamDeviceClient override.
  virtual void ResetPropertyChangedHandler(
      const dbus::ObjectPath& device_path) OVERRIDE {}

  // FlimflamDeviceClient override.
  virtual void GetProperties(const dbus::ObjectPath& device_path,
                             const DictionaryValueCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&FlimflamDeviceClientStubImpl::PassStubDevicePrperties,
                   weak_ptr_factory_.GetWeakPtr(),
                   device_path, callback));
  }

  // FlimflamDeviceClient override.
  virtual base::DictionaryValue* CallGetPropertiesAndBlock(
      const dbus::ObjectPath& device_path) OVERRIDE {
    return new base::DictionaryValue;
  }

  // FlimflamProfileClient override.
  virtual void ProposeScan(const dbus::ObjectPath& device_path,
                           const VoidCallback& callback) OVERRIDE {
    PostVoidCallback(callback, DBUS_METHOD_CALL_SUCCESS);
  }

  // FlimflamDeviceClient override.
  virtual void SetProperty(const dbus::ObjectPath& device_path,
                           const std::string& name,
                           const base::Value& value,
                           const VoidCallback& callback) OVERRIDE {
    base::DictionaryValue* device_properties = NULL;
    if (!stub_devices_.GetDictionary(device_path.value(), &device_properties)) {
      PostVoidCallback(callback, DBUS_METHOD_CALL_FAILURE);
      return;
    }
    device_properties->Set(name, value.DeepCopy());
    PostVoidCallback(callback, DBUS_METHOD_CALL_SUCCESS);
  }

  // FlimflamDeviceClient override.
  virtual void ClearProperty(const dbus::ObjectPath& device_path,
                             const std::string& name,
                             const VoidCallback& callback) OVERRIDE {
    base::DictionaryValue* device_properties = NULL;
    if (!stub_devices_.GetDictionary(device_path.value(), &device_properties)) {
      PostVoidCallback(callback, DBUS_METHOD_CALL_FAILURE);
      return;
    }
    device_properties->Remove(name, NULL);
    PostVoidCallback(callback, DBUS_METHOD_CALL_SUCCESS);
  }

  // FlimflamDeviceClient override.
  virtual void AddIPConfig(const dbus::ObjectPath& device_path,
                           const std::string& method,
                           const ObjectPathCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(FROM_HERE,
                                     base::Bind(callback,
                                                DBUS_METHOD_CALL_SUCCESS,
                                                dbus::ObjectPath()));
  }

  // FlimflamDeviceClient override.
  virtual dbus::ObjectPath CallAddIPConfigAndBlock(
      const dbus::ObjectPath& device_path,
      const std::string& method) OVERRIDE {
    return dbus::ObjectPath();
  }

  // FlimflamDeviceClient override.
  virtual void RequirePin(const dbus::ObjectPath& device_path,
                          const std::string& pin,
                          bool require,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) OVERRIDE {
    MessageLoop::current()->PostTask(FROM_HERE, callback);
  }

  // FlimflamDeviceClient override.
  virtual void EnterPin(const dbus::ObjectPath& device_path,
                        const std::string& pin,
                        const base::Closure& callback,
                        const ErrorCallback& error_callback) OVERRIDE {
    MessageLoop::current()->PostTask(FROM_HERE, callback);
  }

  // FlimflamDeviceClient override.
  virtual void UnblockPin(const dbus::ObjectPath& device_path,
                          const std::string& puk,
                          const std::string& pin,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) OVERRIDE {
    MessageLoop::current()->PostTask(FROM_HERE, callback);
  }

  // FlimflamDeviceClient override.
  virtual void ChangePin(const dbus::ObjectPath& device_path,
                         const std::string& old_pin,
                         const std::string& new_pin,
                         const base::Closure& callback,
                         const ErrorCallback& error_callback) OVERRIDE {
    MessageLoop::current()->PostTask(FROM_HERE, callback);
  }

  // FlimflamDeviceClient override.
  virtual void Register(const dbus::ObjectPath& device_path,
                        const std::string& network_id,
                        const base::Closure& callback,
                        const ErrorCallback& error_callback) OVERRIDE {
    MessageLoop::current()->PostTask(FROM_HERE, callback);
  }

 private:
  void PassStubDevicePrperties(const dbus::ObjectPath& device_path,
                               const DictionaryValueCallback& callback) const {
    base::DictionaryValue* device_properties = NULL;
    if (!stub_devices_.GetDictionary(device_path.value(), &device_properties)) {
      callback.Run(DBUS_METHOD_CALL_FAILURE, base::DictionaryValue());
      return;
    }
    callback.Run(DBUS_METHOD_CALL_SUCCESS, *device_properties);
  }

  // Posts a task to run a void callback with status code |status|.
  void PostVoidCallback(const VoidCallback& callback,
                        DBusMethodCallStatus status) {
    MessageLoop::current()->PostTask(FROM_HERE,
                                     base::Bind(callback, status));
  }

  base::WeakPtrFactory<FlimflamDeviceClientStubImpl> weak_ptr_factory_;
  // Dictionary of <device_name, Dictionary>.
  base::DictionaryValue stub_devices_;

  DISALLOW_COPY_AND_ASSIGN(FlimflamDeviceClientStubImpl);
};

}  // namespace

FlimflamDeviceClient::FlimflamDeviceClient() {}

FlimflamDeviceClient::~FlimflamDeviceClient() {}

// static
FlimflamDeviceClient* FlimflamDeviceClient::Create(
    DBusClientImplementationType type,
    dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new FlimflamDeviceClientImpl(bus);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new FlimflamDeviceClientStubImpl();
}

}  // namespace chromeos
