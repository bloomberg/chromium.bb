// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill_device_client.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/values.h"
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
      : bus_(bus),
        helpers_deleter_(&helpers_) {
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

  virtual TestInterface* GetTestInterface() OVERRIDE {
    return NULL;
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
class ShillDeviceClientStubImpl : public ShillDeviceClient,
                                  public ShillDeviceClient::TestInterface {
 public:
  ShillDeviceClientStubImpl() : weak_ptr_factory_(this) {
    SetDefaultProperties();
  }

  virtual ~ShillDeviceClientStubImpl() {
    STLDeleteContainerPairSecondPointers(
        observer_list_.begin(), observer_list_.end());
  }

  // ShillDeviceClient overrides.

  virtual void AddPropertyChangedObserver(
      const dbus::ObjectPath& device_path,
      ShillPropertyChangedObserver* observer) OVERRIDE {
    GetObserverList(device_path).AddObserver(observer);
  }

  virtual void RemovePropertyChangedObserver(
      const dbus::ObjectPath& device_path,
      ShillPropertyChangedObserver* observer) OVERRIDE {
    GetObserverList(device_path).RemoveObserver(observer);
  }

  virtual void GetProperties(const dbus::ObjectPath& device_path,
                             const DictionaryValueCallback& callback) OVERRIDE {
    if (callback.is_null())
      return;
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ShillDeviceClientStubImpl::PassStubDeviceProperties,
                   weak_ptr_factory_.GetWeakPtr(),
                   device_path, callback));
  }

  virtual base::DictionaryValue* CallGetPropertiesAndBlock(
      const dbus::ObjectPath& device_path) OVERRIDE {
    base::DictionaryValue* device_properties = NULL;
    stub_devices_.GetDictionaryWithoutPathExpansion(
        device_path.value(), &device_properties);
    return device_properties;
  }

  virtual void ProposeScan(const dbus::ObjectPath& device_path,
                           const VoidDBusMethodCallback& callback) OVERRIDE {
    PostVoidCallback(callback, DBUS_METHOD_CALL_SUCCESS);
  }

  virtual void SetProperty(const dbus::ObjectPath& device_path,
                           const std::string& name,
                           const base::Value& value,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) OVERRIDE {
    base::DictionaryValue* device_properties = NULL;
    if (!stub_devices_.GetDictionary(device_path.value(), &device_properties)) {
      std::string error_name("org.chromium.flimflam.Error.Failure");
      std::string error_message("Failed");
      if (!error_callback.is_null()) {
        MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(error_callback,
                                                    error_name,
                                                    error_message));
      }
      return;
    }
    device_properties->Set(name, value.DeepCopy());
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ShillDeviceClientStubImpl::NotifyObserversPropertyChanged,
                   weak_ptr_factory_.GetWeakPtr(), device_path, name));
    if (callback.is_null())
      return;
    MessageLoop::current()->PostTask(FROM_HERE, callback);
  }

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

  virtual void AddIPConfig(
      const dbus::ObjectPath& device_path,
      const std::string& method,
      const ObjectPathDBusMethodCallback& callback) OVERRIDE {
    if (callback.is_null())
      return;
    MessageLoop::current()->PostTask(FROM_HERE,
                                     base::Bind(callback,
                                                DBUS_METHOD_CALL_SUCCESS,
                                                dbus::ObjectPath()));
  }

  virtual void RequirePin(const dbus::ObjectPath& device_path,
                          const std::string& pin,
                          bool require,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) OVERRIDE {
    if (callback.is_null())
      return;
    MessageLoop::current()->PostTask(FROM_HERE, callback);
  }

  virtual void EnterPin(const dbus::ObjectPath& device_path,
                        const std::string& pin,
                        const base::Closure& callback,
                        const ErrorCallback& error_callback) OVERRIDE {
    if (callback.is_null())
      return;
    MessageLoop::current()->PostTask(FROM_HERE, callback);
  }

  virtual void UnblockPin(const dbus::ObjectPath& device_path,
                          const std::string& puk,
                          const std::string& pin,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) OVERRIDE {
    if (callback.is_null())
      return;
    MessageLoop::current()->PostTask(FROM_HERE, callback);
  }

  virtual void ChangePin(const dbus::ObjectPath& device_path,
                         const std::string& old_pin,
                         const std::string& new_pin,
                         const base::Closure& callback,
                         const ErrorCallback& error_callback) OVERRIDE {
    if (callback.is_null())
      return;
    MessageLoop::current()->PostTask(FROM_HERE, callback);
  }

  virtual void Register(const dbus::ObjectPath& device_path,
                        const std::string& network_id,
                        const base::Closure& callback,
                        const ErrorCallback& error_callback) OVERRIDE {
    if (callback.is_null())
      return;
    MessageLoop::current()->PostTask(FROM_HERE, callback);
  }

  virtual void SetCarrier(const dbus::ObjectPath& device_path,
                          const std::string& carrier,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) OVERRIDE {
    if (callback.is_null())
      return;
    MessageLoop::current()->PostTask(FROM_HERE, callback);
  }

  virtual ShillDeviceClient::TestInterface* GetTestInterface() OVERRIDE {
    return this;
  }

  // ShillDeviceClient::TestInterface overrides.

  virtual void AddDevice(const std::string& device_path,
                         const std::string& type,
                         const std::string& object_path) OVERRIDE {
    base::DictionaryValue* properties = GetDeviceProperties(device_path);
    properties->SetWithoutPathExpansion(
        flimflam::kTypeProperty,
        base::Value::CreateStringValue(type));
    properties->SetWithoutPathExpansion(
        flimflam::kDBusObjectProperty,
        base::Value::CreateStringValue(object_path));
    properties->SetWithoutPathExpansion(
        flimflam::kDBusConnectionProperty,
        base::Value::CreateStringValue("/stub"));
  }

  virtual void RemoveDevice(const std::string& device_path) OVERRIDE {
    stub_devices_.RemoveWithoutPathExpansion(device_path, NULL);
  }

  virtual void ClearDevices() OVERRIDE {
    stub_devices_.Clear();
  }

  virtual void SetDeviceProperty(const std::string& device_path,
                                 const std::string& name,
                                 const base::Value& value) {
    SetProperty(dbus::ObjectPath(device_path), name, value,
                base::Bind(&base::DoNothing),
                base::Bind(&ShillDeviceClientStubImpl::ErrorFunction));
  }

 private:
  typedef ObserverList<ShillPropertyChangedObserver> PropertyObserverList;

  void SetDefaultProperties() {
    // Add a wifi device. Note: path matches Manager entry.
    AddDevice("stub_wifi_device1", flimflam::kTypeWifi, "/device/wifi1");

    // Add a cellular device. Used in SMS stub. Note: path matches
    // Manager entry.
    AddDevice("stub_cellular_device1", flimflam::kTypeCellular,
              "/device/cellular1");
  }

  void PassStubDeviceProperties(const dbus::ObjectPath& device_path,
                                const DictionaryValueCallback& callback) const {
    const base::DictionaryValue* device_properties = NULL;
    if (!stub_devices_.GetDictionaryWithoutPathExpansion(
            device_path.value(), &device_properties)) {
      base::DictionaryValue empty_dictionary;
      callback.Run(DBUS_METHOD_CALL_FAILURE, empty_dictionary);
      return;
    }
    callback.Run(DBUS_METHOD_CALL_SUCCESS, *device_properties);
  }

  // Posts a task to run a void callback with status code |status|.
  void PostVoidCallback(const VoidDBusMethodCallback& callback,
                        DBusMethodCallStatus status) {
    if (callback.is_null())
      return;
    MessageLoop::current()->PostTask(FROM_HERE,
                                     base::Bind(callback, status));
  }

  void NotifyObserversPropertyChanged(const dbus::ObjectPath& device_path,
                                      const std::string& property) {
    base::DictionaryValue* dict = NULL;
    std::string path = device_path.value();
    if (!stub_devices_.GetDictionaryWithoutPathExpansion(path, &dict)) {
      LOG(ERROR) << "Notify for unknown service: " << path;
      return;
    }
    base::Value* value = NULL;
    if (!dict->GetWithoutPathExpansion(property, &value)) {
      LOG(ERROR) << "Notify for unknown property: "
                 << path << " : " << property;
      return;
    }
    FOR_EACH_OBSERVER(ShillPropertyChangedObserver,
                      GetObserverList(device_path),
                      OnPropertyChanged(property, *value));
  }

  base::DictionaryValue* GetDeviceProperties(const std::string& device_path) {
    base::DictionaryValue* properties = NULL;
    if (!stub_devices_.GetDictionaryWithoutPathExpansion(
            device_path, &properties)) {
      properties = new base::DictionaryValue;
      stub_devices_.Set(device_path, properties);
    }
    return properties;
  }

  PropertyObserverList& GetObserverList(const dbus::ObjectPath& device_path) {
    std::map<dbus::ObjectPath, PropertyObserverList*>::iterator iter =
        observer_list_.find(device_path);
    if (iter != observer_list_.end())
      return *(iter->second);
    PropertyObserverList* observer_list = new PropertyObserverList();
    observer_list_[device_path] = observer_list;
    return *observer_list;
  }

  static void ErrorFunction(const std::string& error_name,
                            const std::string& error_message) {
    LOG(ERROR) << "Shill Error: " << error_name << " : " << error_message;
  }

  // Dictionary of <device_name, Dictionary>.
  base::DictionaryValue stub_devices_;
  // Observer list for each device.
  std::map<dbus::ObjectPath, PropertyObserverList*> observer_list_;

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
