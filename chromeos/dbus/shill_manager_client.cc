// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill_manager_client.h"

#include "base/bind.h"
#include "base/chromeos/chromeos_version.h"
#include "base/message_loop.h"
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

// Returns whether the properties have the required keys or not.
bool AreServicePropertiesValid(const base::DictionaryValue& properties) {
  if (properties.HasKey(flimflam::kGuidProperty))
    return true;
  return properties.HasKey(flimflam::kTypeProperty) &&
      properties.HasKey(flimflam::kSecurityProperty) &&
      properties.HasKey(flimflam::kSSIDProperty);
}

// Appends a string-to-variant dictionary to the writer.
void AppendServicePropertiesDictionary(
    dbus::MessageWriter* writer,
    const base::DictionaryValue& dictionary) {
  dbus::MessageWriter array_writer(NULL);
  writer->OpenArray("{sv}", &array_writer);
  for (base::DictionaryValue::Iterator it(dictionary);
       it.HasNext();
       it.Advance()) {
    dbus::MessageWriter entry_writer(NULL);
    array_writer.OpenDictEntry(&entry_writer);
    entry_writer.AppendString(it.key());
    ShillClientHelper::AppendValueDataAsVariant(&entry_writer, it.value());
    array_writer.CloseContainer(&entry_writer);
  }
  writer->CloseContainer(&array_writer);
}

// The ShillManagerClient implementation.
class ShillManagerClientImpl : public ShillManagerClient {
 public:
  explicit ShillManagerClientImpl(dbus::Bus* bus)
      : proxy_(bus->GetObjectProxy(
          flimflam::kFlimflamServiceName,
          dbus::ObjectPath(flimflam::kFlimflamServicePath))),
        helper_(bus, proxy_) {
    helper_.MonitorPropertyChanged(flimflam::kFlimflamManagerInterface);
  }

  ////////////////////////////////////
  // ShillManagerClient overrides.
  virtual void AddPropertyChangedObserver(
      ShillPropertyChangedObserver* observer) OVERRIDE {
    helper_.AddPropertyChangedObserver(observer);
  }

  virtual void RemovePropertyChangedObserver(
      ShillPropertyChangedObserver* observer) OVERRIDE {
    helper_.RemovePropertyChangedObserver(observer);
  }

  virtual void GetProperties(const DictionaryValueCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamManagerInterface,
                                 flimflam::kGetPropertiesFunction);
    helper_.CallDictionaryValueMethod(&method_call, callback);
  }

  virtual base::DictionaryValue* CallGetPropertiesAndBlock() OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamManagerInterface,
                                 flimflam::kGetPropertiesFunction);
    return helper_.CallDictionaryValueMethodAndBlock(&method_call);
  }

  virtual void GetNetworksForGeolocation(
      const DictionaryValueCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamManagerInterface,
                                 shill::kGetNetworksForGeolocation);
    helper_.CallDictionaryValueMethod(&method_call, callback);
  }

  virtual void SetProperty(const std::string& name,
                           const base::Value& value,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamManagerInterface,
                                 flimflam::kSetPropertyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    ShillClientHelper::AppendValueDataAsVariant(&writer, value);
    helper_.CallVoidMethodWithErrorCallback(&method_call,
                                            callback,
                                            error_callback);
  }

  virtual void RequestScan(const std::string& type,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamManagerInterface,
                                 flimflam::kRequestScanFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(type);
    helper_.CallVoidMethodWithErrorCallback(&method_call,
                                            callback,
                                            error_callback);
  }

  virtual void EnableTechnology(
      const std::string& type,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamManagerInterface,
                                 flimflam::kEnableTechnologyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(type);
    helper_.CallVoidMethodWithErrorCallback(&method_call,
                                            callback,
                                            error_callback);
  }

  virtual void DisableTechnology(
      const std::string& type,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamManagerInterface,
                                 flimflam::kDisableTechnologyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(type);
    helper_.CallVoidMethodWithErrorCallback(&method_call,
                                            callback,
                                            error_callback);
  }

  virtual void ConfigureService(
      const base::DictionaryValue& properties,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    DCHECK(AreServicePropertiesValid(properties));
    dbus::MethodCall method_call(flimflam::kFlimflamManagerInterface,
                                 flimflam::kConfigureServiceFunction);
    dbus::MessageWriter writer(&method_call);
    AppendServicePropertiesDictionary(&writer, properties);
    helper_.CallVoidMethodWithErrorCallback(&method_call,
                                            callback,
                                            error_callback);
  }

  virtual void GetService(
      const base::DictionaryValue& properties,
      const ObjectPathCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    dbus::MethodCall method_call(flimflam::kFlimflamManagerInterface,
                                 flimflam::kGetServiceFunction);
    dbus::MessageWriter writer(&method_call);
    AppendServicePropertiesDictionary(&writer, properties);
    helper_.CallObjectPathMethodWithErrorCallback(&method_call,
                                                  callback,
                                                  error_callback);
  }

  virtual TestInterface* GetTestInterface() OVERRIDE {
    return NULL;
  }

 private:
  dbus::ObjectProxy* proxy_;
  ShillClientHelper helper_;

  DISALLOW_COPY_AND_ASSIGN(ShillManagerClientImpl);
};

// Used to compare values for finding entries to erase in a ListValue.
// (ListValue only implements a const_iterator version of Find).
struct ValueEquals {
  explicit ValueEquals(const Value* first) : first_(first) {}
  bool operator()(const Value* second) const {
    return first_->Equals(second);
  }
  const Value* first_;
};

// A stub implementation of ShillManagerClient.
// Implemented: Stub devices and services for NetworkStateManager tests.
// Implemented: Stub cellular device entry for SMS tests.
class ShillManagerClientStubImpl : public ShillManagerClient,
                                   public ShillManagerClient::TestInterface {
 public:
  ShillManagerClientStubImpl()
      : weak_ptr_factory_(this) {
    SetDefaultProperties();
  }

  virtual ~ShillManagerClientStubImpl() {}

  // ShillManagerClient overrides.

  virtual void AddPropertyChangedObserver(
      ShillPropertyChangedObserver* observer) OVERRIDE {
    observer_list_.AddObserver(observer);
  }

  virtual void RemovePropertyChangedObserver(
      ShillPropertyChangedObserver* observer) OVERRIDE {
    observer_list_.RemoveObserver(observer);
  }

  virtual void GetProperties(const DictionaryValueCallback& callback) OVERRIDE {
    if (callback.is_null())
      return;
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(
            &ShillManagerClientStubImpl::PassStubProperties,
            weak_ptr_factory_.GetWeakPtr(),
            callback));
  }

  virtual base::DictionaryValue* CallGetPropertiesAndBlock() OVERRIDE {
    return stub_properties_.DeepCopy();
  }

  virtual void GetNetworksForGeolocation(
      const DictionaryValueCallback& callback) OVERRIDE {
    if (callback.is_null())
      return;
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(
            &ShillManagerClientStubImpl::PassStubGeoNetworks,
            weak_ptr_factory_.GetWeakPtr(),
            callback));
  }

  virtual void SetProperty(const std::string& name,
                           const base::Value& value,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) OVERRIDE {
    stub_properties_.Set(name, value.DeepCopy());
    if (callback.is_null())
      return;
    MessageLoop::current()->PostTask(FROM_HERE, callback);
  }

  virtual void RequestScan(const std::string& type,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) OVERRIDE {
    const int kScanDelayMilliseconds = 3000;
    CallNotifyObserversPropertyChanged(
        flimflam::kServicesProperty, kScanDelayMilliseconds);
    if (callback.is_null())
      return;
    MessageLoop::current()->PostTask(FROM_HERE, callback);
  }

  virtual void EnableTechnology(
      const std::string& type,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    base::ListValue* enabled_list = NULL;
    if (!stub_properties_.GetListWithoutPathExpansion(
            flimflam::kEnabledTechnologiesProperty, &enabled_list)) {
      if (!error_callback.is_null()) {
        MessageLoop::current()->PostTask(FROM_HERE, callback);
        MessageLoop::current()->PostTask(
            FROM_HERE,
            base::Bind(error_callback, "StubError", "Property not found"));
      }
      return;
    }
    enabled_list->AppendIfNotPresent(new base::StringValue(type));
    CallNotifyObserversPropertyChanged(
        flimflam::kEnabledTechnologiesProperty, 0);
    if (callback.is_null())
      return;
    MessageLoop::current()->PostTask(FROM_HERE, callback);
  }

  virtual void DisableTechnology(
      const std::string& type,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    base::ListValue* enabled_list = NULL;
    if (!stub_properties_.GetListWithoutPathExpansion(
            flimflam::kEnabledTechnologiesProperty, &enabled_list)) {
      if (!error_callback.is_null()) {
        MessageLoop::current()->PostTask(
            FROM_HERE,
            base::Bind(error_callback, "StubError", "Property not found"));
      }
      return;
    }
    base::StringValue type_value(type);
    enabled_list->Remove(type_value, NULL);
    CallNotifyObserversPropertyChanged(
        flimflam::kEnabledTechnologiesProperty, 0);
    if (callback.is_null())
      return;
    MessageLoop::current()->PostTask(FROM_HERE, callback);
  }

  virtual void ConfigureService(
      const base::DictionaryValue& properties,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    if (callback.is_null())
      return;
    MessageLoop::current()->PostTask(FROM_HERE, callback);
  }

  virtual void GetService(
      const base::DictionaryValue& properties,
      const ObjectPathCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    if (callback.is_null())
      return;
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, dbus::ObjectPath()));
  }

  virtual ShillManagerClient::TestInterface* GetTestInterface() OVERRIDE {
    return this;
  }

  // ShillManagerClient::TestInterface overrides.

  virtual void AddDevice(const std::string& device_path) OVERRIDE {
    if (GetListProperty(flimflam::kDevicesProperty)->AppendIfNotPresent(
            base::Value::CreateStringValue(device_path))) {
      CallNotifyObserversPropertyChanged(flimflam::kDevicesProperty, 0);
    }
  }

  virtual void RemoveDevice(const std::string& device_path) OVERRIDE {
    base::StringValue device_path_value(device_path);
    if (GetListProperty(flimflam::kDevicesProperty)->Remove(
            device_path_value, NULL)) {
      CallNotifyObserversPropertyChanged(flimflam::kDevicesProperty, 0);
    }
  }

  virtual void ResetDevices() OVERRIDE {
    stub_properties_.Remove(flimflam::kDevicesProperty, NULL);
  }

  virtual void AddService(const std::string& service_path,
                          bool add_to_watch_list) OVERRIDE {
    if (GetListProperty(flimflam::kServicesProperty)->AppendIfNotPresent(
            base::Value::CreateStringValue(service_path))) {
      CallNotifyObserversPropertyChanged(flimflam::kServicesProperty, 0);
    }
    if (add_to_watch_list)
      AddServiceToWatchList(service_path);
  }

  virtual void AddServiceAtIndex(const std::string& service_path,
                                 size_t index,
                                 bool add_to_watch_list) OVERRIDE {
    base::StringValue path_value(service_path);
    base::ListValue* service_list =
        GetListProperty(flimflam::kServicesProperty);
    base::ListValue::iterator iter =
        std::find_if(service_list->begin(), service_list->end(),
                     ValueEquals(&path_value));
    service_list->Find(path_value);
    if (iter != service_list->end())
      service_list->Erase(iter, NULL);
    service_list->Insert(index, path_value.DeepCopy());
    CallNotifyObserversPropertyChanged(flimflam::kServicesProperty, 0);
    if (add_to_watch_list)
      AddServiceToWatchList(service_path);
  }

  virtual void RemoveService(const std::string& service_path) OVERRIDE {
    base::StringValue service_path_value(service_path);
    if (GetListProperty(flimflam::kServicesProperty)->Remove(
            service_path_value, NULL)) {
      CallNotifyObserversPropertyChanged(flimflam::kServicesProperty, 0);
    }
    if (GetListProperty(flimflam::kServiceWatchListProperty)->Remove(
            service_path_value, NULL)) {
      CallNotifyObserversPropertyChanged(
          flimflam::kServiceWatchListProperty, 0);
    }
  }

  virtual void AddTechnology(const std::string& type, bool enabled) OVERRIDE {
    if (GetListProperty(flimflam::kAvailableTechnologiesProperty)->
        AppendIfNotPresent(base::Value::CreateStringValue(type))) {
      CallNotifyObserversPropertyChanged(
          flimflam::kAvailableTechnologiesProperty, 0);
    }
    if (enabled &&
        GetListProperty(flimflam::kEnabledTechnologiesProperty)->
        AppendIfNotPresent(base::Value::CreateStringValue(type))) {
      CallNotifyObserversPropertyChanged(
          flimflam::kEnabledTechnologiesProperty, 0);
    }
  }

  virtual void RemoveTechnology(const std::string& type) OVERRIDE {
    base::StringValue type_value(type);
    if (GetListProperty(flimflam::kAvailableTechnologiesProperty)->Remove(
            type_value, NULL)) {
      CallNotifyObserversPropertyChanged(
          flimflam::kAvailableTechnologiesProperty, 0);
    }
    if (GetListProperty(flimflam::kEnabledTechnologiesProperty)->Remove(
            type_value, NULL)) {
      CallNotifyObserversPropertyChanged(
          flimflam::kEnabledTechnologiesProperty, 0);
    }
  }

  virtual void ClearProperties() OVERRIDE {
    stub_properties_.Clear();
  }

  virtual void AddGeoNetwork(const std::string& technology,
                             const base::DictionaryValue& network) OVERRIDE {
    base::ListValue* list_value = NULL;
    if (!stub_geo_networks_.GetListWithoutPathExpansion(
            technology, &list_value)) {
      list_value = new base::ListValue;
      stub_geo_networks_.Set(technology, list_value);
    }
    list_value->Append(network.DeepCopy());
  }

 private:
  void AddServiceToWatchList(const std::string& service_path) {
    if (GetListProperty(
            flimflam::kServiceWatchListProperty)->AppendIfNotPresent(
                base::Value::CreateStringValue(service_path))) {
      CallNotifyObserversPropertyChanged(
          flimflam::kServiceWatchListProperty, 0);
    }
  }

  void SetDefaultProperties() {
    // Stub Devices, Note: names match Device stub map.
    AddDevice("stub_wifi_device1");
    AddDevice("stub_cellular_device1");

    // Stub Services, Note: names match Service stub map.
    AddService("stub_ethernet", true);
    AddService("stub_wifi1", true);
    AddService("stub_wifi2", true);
    AddService("stub_cellular1", true);

    // Stub Technologies
    AddTechnology(flimflam::kTypeEthernet, true);
    AddTechnology(flimflam::kTypeWifi, true);
    AddTechnology(flimflam::kTypeCellular, true);
  }

  void PassStubProperties(const DictionaryValueCallback& callback) const {
    callback.Run(DBUS_METHOD_CALL_SUCCESS, stub_properties_);
  }

  void PassStubGeoNetworks(const DictionaryValueCallback& callback) const {
    callback.Run(DBUS_METHOD_CALL_SUCCESS, stub_geo_networks_);
  }

  void CallNotifyObserversPropertyChanged(const std::string& property,
                                          int delay_ms) {
    // Avoid unnecessary delayed task if we have no observers (e.g. during
    // initial setup).
    if (observer_list_.size() == 0)
      return;
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ShillManagerClientStubImpl::NotifyObserversPropertyChanged,
                   weak_ptr_factory_.GetWeakPtr(),
                   property),
        base::TimeDelta::FromMilliseconds(delay_ms));
  }

  void NotifyObserversPropertyChanged(const std::string& property) {
    base::Value* value = NULL;
    if (!stub_properties_.GetWithoutPathExpansion(property, &value)) {
      LOG(ERROR) << "Notify for unknown property: " << property;
      return;
    }
    FOR_EACH_OBSERVER(ShillPropertyChangedObserver,
                      observer_list_,
                      OnPropertyChanged(property, *value));
  }

  base::ListValue* GetListProperty(const std::string& property) {
    base::ListValue* list_property = NULL;
    if (!stub_properties_.GetListWithoutPathExpansion(
            property, &list_property)) {
      list_property = new base::ListValue;
      stub_properties_.Set(property, list_property);
    }
    return list_property;
  }

  // Dictionary of property name -> property value
  base::DictionaryValue stub_properties_;
  // Dictionary of technology -> list of property dictionaries
  base::DictionaryValue stub_geo_networks_;

  ObserverList<ShillPropertyChangedObserver> observer_list_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ShillManagerClientStubImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShillManagerClientStubImpl);
};

}  // namespace

ShillManagerClient::ShillManagerClient() {}

ShillManagerClient::~ShillManagerClient() {}

// static
ShillManagerClient* ShillManagerClient::Create(
    DBusClientImplementationType type,
    dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new ShillManagerClientImpl(bus);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new ShillManagerClientStubImpl();
}

}  // namespace chromeos
