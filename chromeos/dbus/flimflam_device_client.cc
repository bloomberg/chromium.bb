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
  virtual void RequirePin(const dbus::ObjectPath& device_path,
                          const std::string& pin,
                          bool require,
                          const VoidCallback& callback) OVERRIDE  {
    dbus::MethodCall method_call(flimflam::kFlimflamDeviceInterface,
                                 flimflam::kRequirePinFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(pin);
    writer.AppendBool(require);
    GetHelper(device_path)->CallVoidMethod(&method_call, callback);
  }

  // FlimflamProfileClient override.
  virtual void EnterPin(const dbus::ObjectPath& device_path,
                        const std::string& pin,
                        const VoidCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamDeviceInterface,
                                 flimflam::kEnterPinFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(pin);
    GetHelper(device_path)->CallVoidMethod(&method_call, callback);
  }

  // FlimflamProfileClient override.
  virtual void UnblockPin(const dbus::ObjectPath& device_path,
                          const std::string& puk,
                          const std::string& pin,
                          const VoidCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamDeviceInterface,
                                 flimflam::kUnblockPinFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(puk);
    writer.AppendString(pin);
    GetHelper(device_path)->CallVoidMethod(&method_call, callback);
  }

  // FlimflamProfileClient override.
  virtual void ChangePin(const dbus::ObjectPath& device_path,
                         const std::string& old_pin,
                         const std::string& new_pin,
                         const VoidCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamDeviceInterface,
                                 flimflam::kChangePinFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(old_pin);
    writer.AppendString(new_pin);
    GetHelper(device_path)->CallVoidMethod(&method_call, callback);
  }

  // FlimflamProfileClient override.
  virtual void Register(const dbus::ObjectPath& device_path,
                        const std::string& network_id,
                        const VoidCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamDeviceInterface,
                                 flimflam::kRegisterFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(network_id);
    GetHelper(device_path)->CallVoidMethod(&method_call, callback);
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
class FlimflamDeviceClientStubImpl : public FlimflamDeviceClient {
 public:
  FlimflamDeviceClientStubImpl() : weak_ptr_factory_(this) {}

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
        base::Bind(&FlimflamDeviceClientStubImpl::PassEmptyDictionaryValue,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

  // FlimflamProfileClient override.
  virtual void ProposeScan(const dbus::ObjectPath& device_path,
                           const VoidCallback& callback) OVERRIDE {
    PostSuccessVoidCallback(callback);
  }

  // FlimflamDeviceClient override.
  virtual void SetProperty(const dbus::ObjectPath& device_path,
                           const std::string& name,
                           const base::Value& value,
                           const VoidCallback& callback) OVERRIDE {
    PostSuccessVoidCallback(callback);
  }

  // FlimflamDeviceClient override.
  virtual void ClearProperty(const dbus::ObjectPath& device_path,
                             const std::string& name,
                             const VoidCallback& callback) OVERRIDE {
    PostSuccessVoidCallback(callback);
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
  virtual void RequirePin(const dbus::ObjectPath& device_path,
                          const std::string& pin,
                          bool require,
                          const VoidCallback& callback) OVERRIDE {
    PostSuccessVoidCallback(callback);
  }

  // FlimflamDeviceClient override.
  virtual void EnterPin(const dbus::ObjectPath& device_path,
                        const std::string& pin,
                        const VoidCallback& callback) OVERRIDE {
    PostSuccessVoidCallback(callback);
  }

  // FlimflamDeviceClient override.
  virtual void UnblockPin(const dbus::ObjectPath& device_path,
                          const std::string& puk,
                          const std::string& pin,
                          const VoidCallback& callback) OVERRIDE {
    PostSuccessVoidCallback(callback);
  }

  // FlimflamDeviceClient override.
  virtual void ChangePin(const dbus::ObjectPath& device_path,
                         const std::string& old_pin,
                         const std::string& new_pin,
                         const VoidCallback& callback) OVERRIDE {
    PostSuccessVoidCallback(callback);
  }

  // FlimflamDeviceClient override.
  virtual void Register(const dbus::ObjectPath& device_path,
                        const std::string& network_id,
                        const VoidCallback& callback) OVERRIDE {
    PostSuccessVoidCallback(callback);
  }

 private:
  void PassEmptyDictionaryValue(const DictionaryValueCallback& callback) const {
    base::DictionaryValue dictionary;
    callback.Run(DBUS_METHOD_CALL_SUCCESS, dictionary);
  }

  // Posts a task to run a void callback with success status code.
  void PostSuccessVoidCallback(const VoidCallback& callback) {
    MessageLoop::current()->PostTask(FROM_HERE,
                                     base::Bind(callback,
                                                DBUS_METHOD_CALL_SUCCESS));
  }

  base::WeakPtrFactory<FlimflamDeviceClientStubImpl> weak_ptr_factory_;

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
