// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ap_manager_client.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/observer_list.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_manager.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"

namespace chromeos {

// TODO(benchan): Move these constants to system_api.
namespace apmanager {
const char kApManagerServiceName[] = "org.chromium.apmanager";
const char kApManagerServicePath[] = "/org/chromium/apmanager";
const char kApManagerManagerPath[] = "/org/chromium/apmanager/Manager";
const char kManagerInterfaceName[] = "org.chromium.apmanager.Manager";
const char kConfigInterfaceName[] = "org.chromium.apmanager.Config";
const char kDeviceInterfaceName[] = "org.chromium.apmanager.Device";
const char kServiceInterfaceName[] = "org.chromium.apmanager.Service";
const char kCreateServiceMethod[] = "CreateService";
const char kRemoveServiceMethod[] = "RemoveService";
const char kStartMethod[] = "Start";
const char kStopMethod[] = "Stop";
const char kSsidProperty[] = "Ssid";
const char kInterfaceNameProperty[] = "InterfaceName";
const char kSecurityModeProperty[] = "SecurityMode";
const char kPassphraseProperty[] = "Passphrase";
const char kHwModeProperty[] = "HwMode";
const char kOperationModeProperty[] = "OperationMode";
const char kChannelProperty[] = "Channel";
const char kHiddenNetworkProperty[] = "HiddenNetwork";
const char kBridgeInterfaceProperty[] = "BridgeInterface";
const char kServiceAddressIndexProperty[] = "ServerAddressIndex";
const char kDeviceNameProperty[] = "DeviceName";
const char kInUsedProperty[] = "InUsed";
const char kPreferredApInterfaceProperty[] = "PreferredApInterface";
const char kConfigName[] = "Config";
const char kStateName[] = "State";

}  // namespace apmanager

namespace {

// Since there is no property associated with Manager objects, an empty callback
// is used.
void ManagerPropertyChanged(const std::string& property_name) {
}

// The ApManagerClient implementation used in production.
class ApManagerClientImpl : public ApManagerClient,
                            public dbus::ObjectManager::Interface {
 public:
  ApManagerClientImpl();
  ~ApManagerClientImpl() override;

  // ApManagerClient overrides.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void CreateService(const ObjectPathDBusMethodCallback& callback) override;
  void RemoveService(const dbus::ObjectPath& object_path,
                     const VoidDBusMethodCallback& callback) override;
  void StartService(const dbus::ObjectPath& object_path,
                    const VoidDBusMethodCallback& callback) override;
  void StopService(const dbus::ObjectPath& object_path,
                   const VoidDBusMethodCallback& callback) override;
  ConfigProperties* GetConfigProperties(
      const dbus::ObjectPath& object_path) override;
  const DeviceProperties* GetDeviceProperties(
      const dbus::ObjectPath& object_path) override;
  const ServiceProperties* GetServiceProperties(
      const dbus::ObjectPath& object_path) override;

  // DBusClient overrides.
  void Init(dbus::Bus* bus) override;

  // dbus::ObjectManager::Interface overrides.
  dbus::PropertySet* CreateProperties(
      dbus::ObjectProxy* object_proxy,
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override;
  void ObjectAdded(const dbus::ObjectPath& object_path,
                   const std::string& interface_name) override;
  void ObjectRemoved(const dbus::ObjectPath& object_path,
                     const std::string& interface_name) override;

 private:
  // Called by dbus::PropertySet when a property value is changed,
  // either by result of a signal or response to a GetAll() or Get()
  // call. Informs observers.
  void OnConfigPropertyChanged(const dbus::ObjectPath& object_path,
                               const std::string& property_name);
  void OnDevicePropertyChanged(const dbus::ObjectPath& object_path,
                               const std::string& property_name);
  void OnServicePropertyChanged(const dbus::ObjectPath& object_path,
                                const std::string& property_name);

  void OnObjectPathDBusMethod(const ObjectPathDBusMethodCallback& callback,
                              dbus::Response* response);
  void OnStringDBusMethod(const StringDBusMethodCallback& callback,
                          dbus::Response* response);
  void OnVoidDBusMethod(const VoidDBusMethodCallback& callback,
                        dbus::Response* response);

  // List of observers interested in event notifications from us.
  ObserverList<Observer> observers_;
  dbus::ObjectManager* object_manager_;
  base::WeakPtrFactory<ApManagerClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ApManagerClientImpl);
};

ApManagerClientImpl::ApManagerClientImpl()
    : object_manager_(nullptr), weak_ptr_factory_(this) {
}

ApManagerClientImpl::~ApManagerClientImpl() {
  if (object_manager_) {
    object_manager_->UnregisterInterface(apmanager::kManagerInterfaceName);
    object_manager_->UnregisterInterface(apmanager::kConfigInterfaceName);
    object_manager_->UnregisterInterface(apmanager::kDeviceInterfaceName);
    object_manager_->UnregisterInterface(apmanager::kServiceInterfaceName);
  }
}

void ApManagerClientImpl::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void ApManagerClientImpl::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void ApManagerClientImpl::CreateService(
    const ObjectPathDBusMethodCallback& callback) {
  dbus::ObjectProxy* object_proxy = object_manager_->GetObjectProxy(
      dbus::ObjectPath(apmanager::kApManagerManagerPath));
  if (!object_proxy) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ApManagerClientImpl::OnObjectPathDBusMethod,
                   weak_ptr_factory_.GetWeakPtr(), callback, nullptr));
    return;
  }

  dbus::MethodCall method_call(apmanager::kManagerInterfaceName,
                               apmanager::kCreateServiceMethod);
  dbus::MessageWriter writer(&method_call);
  object_proxy->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&ApManagerClientImpl::OnObjectPathDBusMethod,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void ApManagerClientImpl::RemoveService(
    const dbus::ObjectPath& object_path,
    const VoidDBusMethodCallback& callback) {
  dbus::ObjectProxy* object_proxy = object_manager_->GetObjectProxy(
      dbus::ObjectPath(apmanager::kApManagerManagerPath));
  if (!object_proxy) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ApManagerClientImpl::OnVoidDBusMethod,
                   weak_ptr_factory_.GetWeakPtr(), callback, nullptr));
    return;
  }

  dbus::MethodCall method_call(apmanager::kManagerInterfaceName,
                               apmanager::kRemoveServiceMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendObjectPath(object_path);
  object_proxy->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&ApManagerClientImpl::OnVoidDBusMethod,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void ApManagerClientImpl::StartService(const dbus::ObjectPath& object_path,
                                       const VoidDBusMethodCallback& callback) {
  dbus::ObjectProxy* object_proxy =
      object_manager_->GetObjectProxy(object_path);
  if (!object_proxy) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ApManagerClientImpl::OnVoidDBusMethod,
                   weak_ptr_factory_.GetWeakPtr(), callback, nullptr));
    return;
  }

  dbus::MethodCall method_call(apmanager::kServiceInterfaceName,
                               apmanager::kStartMethod);
  dbus::MessageWriter writer(&method_call);
  object_proxy->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&ApManagerClientImpl::OnVoidDBusMethod,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void ApManagerClientImpl::StopService(const dbus::ObjectPath& object_path,
                                      const VoidDBusMethodCallback& callback) {
  dbus::ObjectProxy* object_proxy =
      object_manager_->GetObjectProxy(object_path);
  if (!object_proxy) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ApManagerClientImpl::OnVoidDBusMethod,
                   weak_ptr_factory_.GetWeakPtr(), callback, nullptr));
    return;
  }

  dbus::MethodCall method_call(apmanager::kServiceInterfaceName,
                               apmanager::kStopMethod);
  dbus::MessageWriter writer(&method_call);
  object_proxy->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&ApManagerClientImpl::OnVoidDBusMethod,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

ApManagerClient::ConfigProperties* ApManagerClientImpl::GetConfigProperties(
    const dbus::ObjectPath& object_path) {
  return static_cast<ConfigProperties*>(object_manager_->GetProperties(
      object_path, apmanager::kConfigInterfaceName));
}

const ApManagerClient::DeviceProperties*
ApManagerClientImpl::GetDeviceProperties(const dbus::ObjectPath& object_path) {
  return static_cast<DeviceProperties*>(object_manager_->GetProperties(
      object_path, apmanager::kDeviceInterfaceName));
}

const ApManagerClient::ServiceProperties*
ApManagerClientImpl::GetServiceProperties(const dbus::ObjectPath& object_path) {
  return static_cast<ServiceProperties*>(object_manager_->GetProperties(
      object_path, apmanager::kServiceInterfaceName));
}

void ApManagerClientImpl::Init(dbus::Bus* bus) {
  object_manager_ =
      bus->GetObjectManager(apmanager::kApManagerServiceName,
                            dbus::ObjectPath(apmanager::kApManagerServicePath));
  object_manager_->RegisterInterface(apmanager::kManagerInterfaceName, this);
  object_manager_->RegisterInterface(apmanager::kConfigInterfaceName, this);
  object_manager_->RegisterInterface(apmanager::kDeviceInterfaceName, this);
  object_manager_->RegisterInterface(apmanager::kServiceInterfaceName, this);
}

dbus::PropertySet* ApManagerClientImpl::CreateProperties(
    dbus::ObjectProxy* object_proxy,
    const dbus::ObjectPath& object_path,
    const std::string& interface_name) {
  dbus::PropertySet* properties = nullptr;
  if (interface_name == apmanager::kManagerInterfaceName) {
    properties = new dbus::PropertySet(object_proxy, interface_name,
                                       base::Bind(&ManagerPropertyChanged));
  } else if (interface_name == apmanager::kConfigInterfaceName) {
    properties = new ConfigProperties(
        object_proxy, interface_name,
        base::Bind(&ApManagerClientImpl::OnConfigPropertyChanged,
                   weak_ptr_factory_.GetWeakPtr(), object_path));
  } else if (interface_name == apmanager::kDeviceInterfaceName) {
    properties = new DeviceProperties(
        object_proxy, interface_name,
        base::Bind(&ApManagerClientImpl::OnDevicePropertyChanged,
                   weak_ptr_factory_.GetWeakPtr(), object_path));
  } else if (interface_name == apmanager::kServiceInterfaceName) {
    properties = new ServiceProperties(
        object_proxy, interface_name,
        base::Bind(&ApManagerClientImpl::OnServicePropertyChanged,
                   weak_ptr_factory_.GetWeakPtr(), object_path));
  } else {
    NOTREACHED() << "Unhandled interface name " << interface_name;
  }
  return properties;
}

void ApManagerClientImpl::ObjectAdded(const dbus::ObjectPath& object_path,
                                      const std::string& interface_name) {
  if (interface_name == apmanager::kManagerInterfaceName) {
    FOR_EACH_OBSERVER(Observer, observers_, ManagerAdded());
  } else if (interface_name == apmanager::kConfigInterfaceName) {
    FOR_EACH_OBSERVER(Observer, observers_, ConfigAdded(object_path));
  } else if (interface_name == apmanager::kDeviceInterfaceName) {
    FOR_EACH_OBSERVER(Observer, observers_, DeviceAdded(object_path));
  } else if (interface_name == apmanager::kServiceInterfaceName) {
    FOR_EACH_OBSERVER(Observer, observers_, ServiceAdded(object_path));
  } else {
    NOTREACHED() << "Unhandled interface name " << interface_name;
  }
}

void ApManagerClientImpl::ObjectRemoved(const dbus::ObjectPath& object_path,
                                        const std::string& interface_name) {
  if (interface_name == apmanager::kManagerInterfaceName) {
    FOR_EACH_OBSERVER(Observer, observers_, ManagerRemoved());
  } else if (interface_name == apmanager::kConfigInterfaceName) {
    FOR_EACH_OBSERVER(Observer, observers_, ConfigRemoved(object_path));
  } else if (interface_name == apmanager::kDeviceInterfaceName) {
    FOR_EACH_OBSERVER(Observer, observers_, DeviceRemoved(object_path));
  } else if (interface_name == apmanager::kServiceInterfaceName) {
    FOR_EACH_OBSERVER(Observer, observers_, ServiceRemoved(object_path));
  } else {
    NOTREACHED() << "Unhandled interface name " << interface_name;
  }
}

void ApManagerClientImpl::OnConfigPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  FOR_EACH_OBSERVER(Observer, observers_,
                    ConfigPropertyChanged(object_path, property_name));
}

void ApManagerClientImpl::OnDevicePropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  FOR_EACH_OBSERVER(Observer, observers_,
                    ConfigPropertyChanged(object_path, property_name));
}

void ApManagerClientImpl::OnServicePropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  FOR_EACH_OBSERVER(Observer, observers_,
                    ServicePropertyChanged(object_path, property_name));
}

void ApManagerClientImpl::OnObjectPathDBusMethod(
    const ObjectPathDBusMethodCallback& callback,
    dbus::Response* response) {
  if (!response) {
    callback.Run(DBUS_METHOD_CALL_FAILURE, dbus::ObjectPath());
    return;
  }

  dbus::MessageReader reader(response);
  dbus::ObjectPath result;
  if (!reader.PopObjectPath(&result)) {
    callback.Run(DBUS_METHOD_CALL_FAILURE, result);
    return;
  }

  callback.Run(DBUS_METHOD_CALL_SUCCESS, result);
}

void ApManagerClientImpl::OnStringDBusMethod(
    const StringDBusMethodCallback& callback,
    dbus::Response* response) {
  if (!response) {
    callback.Run(DBUS_METHOD_CALL_FAILURE, std::string());
    return;
  }

  dbus::MessageReader reader(response);
  std::string result;
  if (!reader.PopString(&result)) {
    callback.Run(DBUS_METHOD_CALL_FAILURE, std::string());
    return;
  }

  callback.Run(DBUS_METHOD_CALL_SUCCESS, result);
}

void ApManagerClientImpl::OnVoidDBusMethod(
    const VoidDBusMethodCallback& callback,
    dbus::Response* response) {
  callback.Run(response ? DBUS_METHOD_CALL_SUCCESS : DBUS_METHOD_CALL_FAILURE);
}

}  // namespace

ApManagerClient::ConfigProperties::ConfigProperties(
    dbus::ObjectProxy* object_proxy,
    const std::string& dbus_interface_name,
    const PropertyChangedCallback& callback)
    : dbus::PropertySet(object_proxy, dbus_interface_name, callback) {
  RegisterProperty(apmanager::kSsidProperty, &ssid_);
  RegisterProperty(apmanager::kInterfaceNameProperty, &interface_name_);
  RegisterProperty(apmanager::kSecurityModeProperty, &security_mode_);
  RegisterProperty(apmanager::kPassphraseProperty, &passphrase_);
  RegisterProperty(apmanager::kHwModeProperty, &hw_mode_);
  RegisterProperty(apmanager::kOperationModeProperty, &operation_mode_);
  RegisterProperty(apmanager::kChannelProperty, &channel_);
  RegisterProperty(apmanager::kHiddenNetworkProperty, &hidden_network_);
  RegisterProperty(apmanager::kBridgeInterfaceProperty, &bridge_interface_);
  RegisterProperty(apmanager::kServiceAddressIndexProperty,
                   &server_address_index_);
}

ApManagerClient::ConfigProperties::~ConfigProperties() {
}

ApManagerClient::DeviceProperties::DeviceProperties(
    dbus::ObjectProxy* object_proxy,
    const std::string& interface_name,
    const PropertyChangedCallback& callback)
    : dbus::PropertySet(object_proxy, interface_name, callback) {
  RegisterProperty(apmanager::kDeviceNameProperty, &device_name_);
  RegisterProperty(apmanager::kInUsedProperty, &in_used_);
  RegisterProperty(apmanager::kPreferredApInterfaceProperty,
                   &preferred_ap_interface_);
}

ApManagerClient::DeviceProperties::~DeviceProperties() {
}

ApManagerClient::ServiceProperties::ServiceProperties(
    dbus::ObjectProxy* object_proxy,
    const std::string& interface_name,
    const PropertyChangedCallback& callback)
    : dbus::PropertySet(object_proxy, interface_name, callback) {
  RegisterProperty(apmanager::kConfigName, &config_);
  RegisterProperty(apmanager::kStateName, &state_);
}

ApManagerClient::ServiceProperties::~ServiceProperties() {
}

ApManagerClient::Observer::~Observer() {
}

void ApManagerClient::Observer::ManagerAdded() {
}

void ApManagerClient::Observer::ManagerRemoved() {
}

void ApManagerClient::Observer::ConfigAdded(
    const dbus::ObjectPath& object_path) {
}

void ApManagerClient::Observer::ConfigRemoved(
    const dbus::ObjectPath& object_path) {
}

void ApManagerClient::Observer::DeviceAdded(
    const dbus::ObjectPath& object_path) {
}

void ApManagerClient::Observer::DeviceRemoved(
    const dbus::ObjectPath& object_path) {
}

void ApManagerClient::Observer::ServiceAdded(
    const dbus::ObjectPath& object_path) {
}

void ApManagerClient::Observer::ServiceRemoved(
    const dbus::ObjectPath& object_path) {
}

void ApManagerClient::Observer::ConfigPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
}

void ApManagerClient::Observer::DevicePropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
}

void ApManagerClient::Observer::ServicePropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
}

ApManagerClient::ApManagerClient() {
}

ApManagerClient::~ApManagerClient() {
}

// static
ApManagerClient* ApManagerClient::Create() {
  return new ApManagerClientImpl();
}

}  // namespace chromeos
