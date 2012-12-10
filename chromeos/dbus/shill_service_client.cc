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

// A stub implementation of ShillServiceClient.
class ShillServiceClientStubImpl : public ShillServiceClient,
                                   public ShillServiceClient::TestInterface {
 public:
  ShillServiceClientStubImpl() : weak_ptr_factory_(this) {
    SetDefaultProperties();
  }

  virtual ~ShillServiceClientStubImpl() {
    STLDeleteContainerPairSecondPointers(
        observer_list_.begin(), observer_list_.end());
  }

  // ShillServiceClient overrides.

  virtual void AddPropertyChangedObserver(
      const dbus::ObjectPath& service_path,
      ShillPropertyChangedObserver* observer) OVERRIDE {
    GetObserverList(service_path).AddObserver(observer);
  }

  virtual void RemovePropertyChangedObserver(
      const dbus::ObjectPath& service_path,
      ShillPropertyChangedObserver* observer) OVERRIDE {
    GetObserverList(service_path).RemoveObserver(observer);
  }

  virtual void GetProperties(const dbus::ObjectPath& service_path,
                             const DictionaryValueCallback& callback) OVERRIDE {
    if (callback.is_null())
      return;
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ShillServiceClientStubImpl::PassStubDictionaryValue,
                   weak_ptr_factory_.GetWeakPtr(),
                   service_path,
                   callback));
  }

  virtual void SetProperty(const dbus::ObjectPath& service_path,
                           const std::string& name,
                           const base::Value& value,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) OVERRIDE {
    base::DictionaryValue* dict = NULL;
    if (!stub_services_.GetDictionaryWithoutPathExpansion(
            service_path.value(), &dict)) {
      error_callback.Run("StubError", "Service not found");
      return;
    }
    dict->SetWithoutPathExpansion(name, value.DeepCopy());
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ShillServiceClientStubImpl::NotifyObserversPropertyChanged,
                   weak_ptr_factory_.GetWeakPtr(), service_path, name));
    if (callback.is_null())
      return;
    MessageLoop::current()->PostTask(FROM_HERE, callback);
  }

  virtual void ClearProperty(const dbus::ObjectPath& service_path,
                             const std::string& name,
                             const base::Closure& callback,
                             const ErrorCallback& error_callback) OVERRIDE {
    base::DictionaryValue* dict = NULL;
    if (!stub_services_.GetDictionaryWithoutPathExpansion(
            service_path.value(), &dict)) {
      error_callback.Run("StubError", "Service not found");
      return;
    }
    dict->Remove(name, NULL);
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ShillServiceClientStubImpl::NotifyObserversPropertyChanged,
                   weak_ptr_factory_.GetWeakPtr(), service_path, name));
    if (callback.is_null())
      return;
    MessageLoop::current()->PostTask(FROM_HERE, callback);
  }

  virtual void ClearProperties(const dbus::ObjectPath& service_path,
                               const std::vector<std::string>& names,
                               const ListValueCallback& callback,
                               const ErrorCallback& error_callback) OVERRIDE {
    base::DictionaryValue* dict = NULL;
    if (!stub_services_.GetDictionaryWithoutPathExpansion(
            service_path.value(), &dict)) {
      error_callback.Run("StubError", "Service not found");
      return;
    }
    scoped_ptr<base::ListValue> results(new base::ListValue);
    for (std::vector<std::string>::const_iterator iter = names.begin();
         iter != names.end(); ++iter) {
      dict->Remove(*iter, NULL);
      results->AppendBoolean(true);
    }
    for (std::vector<std::string>::const_iterator iter = names.begin();
         iter != names.end(); ++iter) {
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(
              &ShillServiceClientStubImpl::NotifyObserversPropertyChanged,
              weak_ptr_factory_.GetWeakPtr(), service_path, *iter));
    }
    if (callback.is_null())
      return;
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ShillServiceClientStubImpl::PassStubListValue,
                   callback, base::Owned(results.release())));
  }

  virtual void Connect(const dbus::ObjectPath& service_path,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) OVERRIDE {
    // Set Associating
    base::StringValue associating_value(flimflam::kStateAssociation);
    SetServiceProperty(service_path.value(),
                       flimflam::kStateProperty,
                       associating_value);
    // Set Online after a delay
    const int kConnectDelaySeconds = 5;
    base::StringValue online_value(flimflam::kStateOnline);
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ShillServiceClientStubImpl::SetProperty,
                   weak_ptr_factory_.GetWeakPtr(),
                   service_path,
                   flimflam::kStateProperty,
                   online_value,
                   callback, error_callback),
        base::TimeDelta::FromSeconds(kConnectDelaySeconds));
  }

  virtual void Disconnect(const dbus::ObjectPath& service_path,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) OVERRIDE {
    // Set Idle after a delay
    const int kConnectDelaySeconds = 2;
    base::StringValue idle_value(flimflam::kStateIdle);
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ShillServiceClientStubImpl::SetProperty,
                   weak_ptr_factory_.GetWeakPtr(),
                   service_path,
                   flimflam::kStateProperty,
                   idle_value,
                   callback, error_callback),
        base::TimeDelta::FromSeconds(kConnectDelaySeconds));
  }

  virtual void Remove(const dbus::ObjectPath& service_path,
                      const base::Closure& callback,
                      const ErrorCallback& error_callback) OVERRIDE {
    if (callback.is_null())
      return;
    MessageLoop::current()->PostTask(FROM_HERE, callback);
  }

  virtual void ActivateCellularModem(
      const dbus::ObjectPath& service_path,
      const std::string& carrier,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    if (callback.is_null())
      return;
    MessageLoop::current()->PostTask(FROM_HERE, callback);
  }

  virtual bool CallActivateCellularModemAndBlock(
      const dbus::ObjectPath& service_path,
      const std::string& carrier) OVERRIDE {
    return true;
  }

  virtual ShillServiceClient::TestInterface* GetTestInterface() OVERRIDE {
    return this;
  }

  // ShillServiceClient::TestInterface overrides.

  virtual void AddService(const std::string& service_path,
                          const std::string& name,
                          const std::string& type,
                          const std::string& state) OVERRIDE {
    base::DictionaryValue* properties = GetServiceProperties(service_path);
    properties->SetWithoutPathExpansion(
        flimflam::kSSIDProperty,
        base::Value::CreateStringValue(service_path));
    properties->SetWithoutPathExpansion(
        flimflam::kNameProperty,
        base::Value::CreateStringValue(name));
    properties->SetWithoutPathExpansion(
        flimflam::kTypeProperty,
        base::Value::CreateStringValue(type));
    properties->SetWithoutPathExpansion(
        flimflam::kStateProperty,
        base::Value::CreateStringValue(state));
  }

  virtual void RemoveService(const std::string& service_path) {
    stub_services_.RemoveWithoutPathExpansion(service_path, NULL);
  }

  virtual void SetServiceProperty(const std::string& service_path,
                                  const std::string& property,
                                  const base::Value& value) OVERRIDE {
    SetProperty(dbus::ObjectPath(service_path), property, value,
                base::Bind(&base::DoNothing),
                base::Bind(&ShillServiceClientStubImpl::ErrorFunction));
  }

  virtual void ClearServices() OVERRIDE {
    stub_services_.Clear();
  }

 private:
  typedef ObserverList<ShillPropertyChangedObserver> PropertyObserverList;

  void SetDefaultProperties() {
    // Add stub services. Note: names match Manager stub impl.
    AddService("stub_ethernet", "eth0",
               flimflam::kTypeEthernet,
               flimflam::kStateOnline);

    AddService("stub_wifi1", "wifi1",
               flimflam::kTypeWifi,
               flimflam::kStateOnline);

    AddService("stub_wifi2", "wifi2_PSK",
               flimflam::kTypeWifi,
               flimflam::kStateIdle);
    base::StringValue psk_value(flimflam::kSecurityPsk);
    SetServiceProperty("stub_wifi2",
                       flimflam::kSecurityProperty,
                       psk_value);

    AddService("stub_cellular1", "cellular1",
               flimflam::kTypeCellular,
               flimflam::kStateIdle);
    base::StringValue technology_value(flimflam::kNetworkTechnologyGsm);
    SetServiceProperty("stub_cellular1",
                       flimflam::kNetworkTechnologyProperty,
                       technology_value);
  }

  void PassStubDictionaryValue(const dbus::ObjectPath& service_path,
                               const DictionaryValueCallback& callback) {
    base::DictionaryValue* dict = NULL;
    if (!stub_services_.GetDictionaryWithoutPathExpansion(
            service_path.value(), &dict)) {
      base::DictionaryValue empty_dictionary;
      callback.Run(DBUS_METHOD_CALL_FAILURE, empty_dictionary);
      return;
    }
    callback.Run(DBUS_METHOD_CALL_SUCCESS, *dict);
  }

  static void PassStubListValue(const ListValueCallback& callback,
                                base::ListValue* value) {
    callback.Run(*value);
  }

  void NotifyObserversPropertyChanged(const dbus::ObjectPath& service_path,
                                      const std::string& property) {
    base::DictionaryValue* dict = NULL;
    std::string path = service_path.value();
    if (!stub_services_.GetDictionaryWithoutPathExpansion(path, &dict)) {
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
                      GetObserverList(service_path),
                      OnPropertyChanged(property, *value));
  }

  base::DictionaryValue* GetServiceProperties(const std::string& service_path) {
    base::DictionaryValue* properties = NULL;
    if (!stub_services_.GetDictionaryWithoutPathExpansion(
            service_path, &properties)) {
      properties = new base::DictionaryValue;
      stub_services_.Set(service_path, properties);
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

  base::DictionaryValue stub_services_;
  // Observer list for each service.
  std::map<dbus::ObjectPath, PropertyObserverList*> observer_list_;

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
