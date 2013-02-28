// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill_device_client_stub.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_property_changed_observer.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

void ErrorFunction(const std::string& error_name,
                   const std::string& error_message) {
  LOG(ERROR) << "Shill Error: " << error_name << " : " << error_message;
}

}  // namespace

ShillDeviceClientStub::ShillDeviceClientStub() : weak_ptr_factory_(this) {
  SetDefaultProperties();
}

ShillDeviceClientStub::~ShillDeviceClientStub() {
  STLDeleteContainerPairSecondPointers(
      observer_list_.begin(), observer_list_.end());
}

// ShillDeviceClient overrides.

void ShillDeviceClientStub::AddPropertyChangedObserver(
    const dbus::ObjectPath& device_path,
    ShillPropertyChangedObserver* observer){
  GetObserverList(device_path).AddObserver(observer);
}

void ShillDeviceClientStub::RemovePropertyChangedObserver(
    const dbus::ObjectPath& device_path,
    ShillPropertyChangedObserver* observer){
  GetObserverList(device_path).RemoveObserver(observer);
}

void ShillDeviceClientStub::GetProperties(
    const dbus::ObjectPath& device_path,
    const DictionaryValueCallback& callback){
  if (callback.is_null())
    return;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ShillDeviceClientStub::PassStubDeviceProperties,
                 weak_ptr_factory_.GetWeakPtr(),
                 device_path, callback));
}

base::DictionaryValue* ShillDeviceClientStub::CallGetPropertiesAndBlock(
    const dbus::ObjectPath& device_path){
  base::DictionaryValue* device_properties = NULL;
  stub_devices_.GetDictionaryWithoutPathExpansion(
      device_path.value(), &device_properties);
  return device_properties;
}

void ShillDeviceClientStub::ProposeScan(const dbus::ObjectPath& device_path,
                                        const VoidDBusMethodCallback& callback){
  PostVoidCallback(callback, DBUS_METHOD_CALL_SUCCESS);
}

void ShillDeviceClientStub::SetProperty(const dbus::ObjectPath& device_path,
                                        const std::string& name,
                                        const base::Value& value,
                                        const base::Closure& callback,
                                        const ErrorCallback& error_callback){
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
      base::Bind(&ShillDeviceClientStub::NotifyObserversPropertyChanged,
                 weak_ptr_factory_.GetWeakPtr(), device_path, name));
  if (callback.is_null())
    return;
  MessageLoop::current()->PostTask(FROM_HERE, callback);
}

void ShillDeviceClientStub::ClearProperty(
    const dbus::ObjectPath& device_path,
    const std::string& name,
    const VoidDBusMethodCallback& callback){
  base::DictionaryValue* device_properties = NULL;
  if (!stub_devices_.GetDictionary(device_path.value(), &device_properties)) {
    PostVoidCallback(callback, DBUS_METHOD_CALL_FAILURE);
    return;
  }
  device_properties->Remove(name, NULL);
  PostVoidCallback(callback, DBUS_METHOD_CALL_SUCCESS);
}

void ShillDeviceClientStub::AddIPConfig(
    const dbus::ObjectPath& device_path,
    const std::string& method,
    const ObjectPathDBusMethodCallback& callback){
  if (callback.is_null())
    return;
  MessageLoop::current()->PostTask(FROM_HERE,
                                   base::Bind(callback,
                                              DBUS_METHOD_CALL_SUCCESS,
                                              dbus::ObjectPath()));
}

void ShillDeviceClientStub::RequirePin(const dbus::ObjectPath& device_path,
                                       const std::string& pin,
                                       bool require,
                                       const base::Closure& callback,
                                       const ErrorCallback& error_callback){
  if (callback.is_null())
    return;
  MessageLoop::current()->PostTask(FROM_HERE, callback);
}

void ShillDeviceClientStub::EnterPin(const dbus::ObjectPath& device_path,
                                     const std::string& pin,
                                     const base::Closure& callback,
                                     const ErrorCallback& error_callback){
  if (callback.is_null())
    return;
  MessageLoop::current()->PostTask(FROM_HERE, callback);
}

void ShillDeviceClientStub::UnblockPin(const dbus::ObjectPath& device_path,
                                       const std::string& puk,
                                       const std::string& pin,
                                       const base::Closure& callback,
                                       const ErrorCallback& error_callback){
  if (callback.is_null())
    return;
  MessageLoop::current()->PostTask(FROM_HERE, callback);
}

void ShillDeviceClientStub::ChangePin(const dbus::ObjectPath& device_path,
                                      const std::string& old_pin,
                                      const std::string& new_pin,
                                      const base::Closure& callback,
                                      const ErrorCallback& error_callback){
  if (callback.is_null())
    return;
  MessageLoop::current()->PostTask(FROM_HERE, callback);
}

void ShillDeviceClientStub::Register(const dbus::ObjectPath& device_path,
                                     const std::string& network_id,
                                     const base::Closure& callback,
                                     const ErrorCallback& error_callback){
  if (callback.is_null())
    return;
  MessageLoop::current()->PostTask(FROM_HERE, callback);
}

void ShillDeviceClientStub::SetCarrier(const dbus::ObjectPath& device_path,
                                       const std::string& carrier,
                                       const base::Closure& callback,
                                       const ErrorCallback& error_callback){
  if (callback.is_null())
    return;
  MessageLoop::current()->PostTask(FROM_HERE, callback);
}

void ShillDeviceClientStub::Reset(const dbus::ObjectPath& device_path,
                                  const base::Closure& callback,
                                  const ErrorCallback& error_callback){
  if (callback.is_null())
    return;
  MessageLoop::current()->PostTask(FROM_HERE, callback);
}

ShillDeviceClient::TestInterface* ShillDeviceClientStub::GetTestInterface(){
  return this;
}

// ShillDeviceClient::TestInterface overrides.

void ShillDeviceClientStub::AddDevice(const std::string& device_path,
                                      const std::string& type,
                                      const std::string& object_path){
  DBusThreadManager::Get()->GetShillManagerClient()->GetTestInterface()->
      AddDevice(device_path);

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

void ShillDeviceClientStub::RemoveDevice(const std::string& device_path){
  DBusThreadManager::Get()->GetShillManagerClient()->GetTestInterface()->
      RemoveDevice(device_path);

  stub_devices_.RemoveWithoutPathExpansion(device_path, NULL);
}

void ShillDeviceClientStub::ClearDevices(){
  DBusThreadManager::Get()->GetShillManagerClient()->GetTestInterface()->
      ClearDevices();

  stub_devices_.Clear();
}

void ShillDeviceClientStub::SetDeviceProperty(const std::string& device_path,
                                              const std::string& name,
                                              const base::Value& value){
  SetProperty(dbus::ObjectPath(device_path), name, value,
              base::Bind(&base::DoNothing),
              base::Bind(&ErrorFunction));
}

void ShillDeviceClientStub::SetDefaultProperties() {
  // Add a wifi device. Note: path matches Manager entry.
  AddDevice("stub_wifi_device1", flimflam::kTypeWifi, "/device/wifi1");

  // Add a cellular device. Used in SMS stub. Note: path matches
  // Manager entry.
  AddDevice("stub_cellular_device1", flimflam::kTypeCellular,
            "/device/cellular1");
}

void ShillDeviceClientStub::PassStubDeviceProperties(
    const dbus::ObjectPath& device_path,
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
void ShillDeviceClientStub::PostVoidCallback(
    const VoidDBusMethodCallback& callback,
    DBusMethodCallStatus status) {
  if (callback.is_null())
    return;
  MessageLoop::current()->PostTask(FROM_HERE,
                                   base::Bind(callback, status));
}

void ShillDeviceClientStub::NotifyObserversPropertyChanged(
    const dbus::ObjectPath& device_path,
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

base::DictionaryValue* ShillDeviceClientStub::GetDeviceProperties(
    const std::string& device_path) {
  base::DictionaryValue* properties = NULL;
  if (!stub_devices_.GetDictionaryWithoutPathExpansion(
      device_path, &properties)) {
    properties = new base::DictionaryValue;
    stub_devices_.Set(device_path, properties);
  }
  return properties;
}

ShillDeviceClientStub::PropertyObserverList&
ShillDeviceClientStub::GetObserverList(const dbus::ObjectPath& device_path) {
  std::map<dbus::ObjectPath, PropertyObserverList*>::iterator iter =
      observer_list_.find(device_path);
  if (iter != observer_list_.end())
    return *(iter->second);
  PropertyObserverList* observer_list = new PropertyObserverList();
  observer_list_[device_path] = observer_list;
  return *observer_list;
}

}  // namespace chromeos
