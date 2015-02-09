// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/peer_daemon_manager_client.h"

#include "base/bind.h"
#include "base/callback.h"
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
namespace {

// TODO(benchan): Move these constants to system_api.
namespace peerd {
const char kPeerdServiceName[] = "org.chromium.peerd";
const char kPeerdObjectManagerServicePath[] = "/org/chromium/peerd";
const char kPeerdManagerPath[] = "/org/chromium/peerd/Manager";
const char kManagerInterface[] = "org.chromium.peerd.Manager";
const char kServiceInterface[] = "org.chromium.peerd.Service";
const char kPeerInterface[] = "org.chromium.peerd.Peer";
const char kStartMonitoringMethod[] = "StartMonitoring";
const char kStopMonitoringMethod[] = "StopMonitoring";
const char kExposeServiceMethod[] = "ExposeService";
const char kRemoveExposedServiceMethod[] = "RemoveExposedService";
const char kPingMethod[] = "Ping";
}  // namespace peerd

// The PeerDaemonManagerClient implementation used in production.
class PeerDaemonManagerClientImpl : public PeerDaemonManagerClient,
                                    public dbus::ObjectManager::Interface {
 public:
  PeerDaemonManagerClientImpl();
  ~PeerDaemonManagerClientImpl() override;

  // DBusClient overrides.
  void Init(dbus::Bus* bus) override;

  // PeerDaemonManagerClient overrides.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  std::vector<dbus::ObjectPath> GetServices() override;
  std::vector<dbus::ObjectPath> GetPeers() override;
  ServiceProperties* GetServiceProperties(
      const dbus::ObjectPath& object_path) override;
  PeerProperties* GetPeerProperties(
      const dbus::ObjectPath& object_path) override;
  void StartMonitoring(
      const std::vector<std::string>& requested_technologies,
      const base::DictionaryValue& options,
      const StringDBusMethodCallback& callback) override;
  void StopMonitoring(const std::string& monitoring_token,
                      const VoidDBusMethodCallback& callback) override;
  void ExposeService(
      const std::string& service_id,
      const std::map<std::string, std::string>& service_info,
      const base::DictionaryValue& options,
      const StringDBusMethodCallback& callback) override;
  void RemoveExposedService(const std::string& service_token,
                            const VoidDBusMethodCallback& callback) override;
  void Ping(const StringDBusMethodCallback& callback) override;

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
  void OnStringDBusMethod(const StringDBusMethodCallback& callback,
                          dbus::Response* response);
  void OnVoidDBusMethod(const VoidDBusMethodCallback& callback,
                        dbus::Response* response);
  void OnManagerPropertyChanged(const std::string& property_name);
  void OnServicePropertyChanged(const dbus::ObjectPath& object_path,
                                const std::string& property_name);
  void OnPeerPropertyChanged(const dbus::ObjectPath& object_path,
                             const std::string& property_name);

  // List of observers interested in event notifications from us.
  ObserverList<Observer> observers_;
  dbus::ObjectManager* object_manager_;

  base::WeakPtrFactory<PeerDaemonManagerClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PeerDaemonManagerClientImpl);
};

PeerDaemonManagerClientImpl::PeerDaemonManagerClientImpl()
    : object_manager_(nullptr), weak_ptr_factory_(this) {
}

PeerDaemonManagerClientImpl::~PeerDaemonManagerClientImpl() {
  if (object_manager_) {
    object_manager_->UnregisterInterface(peerd::kManagerInterface);
    object_manager_->UnregisterInterface(peerd::kServiceInterface);
    object_manager_->UnregisterInterface(peerd::kPeerInterface);
  }
}

void PeerDaemonManagerClientImpl::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void PeerDaemonManagerClientImpl::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

std::vector<dbus::ObjectPath> PeerDaemonManagerClientImpl::GetServices() {
  return object_manager_->GetObjectsWithInterface(peerd::kServiceInterface);
}

std::vector<dbus::ObjectPath> PeerDaemonManagerClientImpl::GetPeers() {
  return object_manager_->GetObjectsWithInterface(peerd::kPeerInterface);
}

PeerDaemonManagerClient::ServiceProperties*
PeerDaemonManagerClientImpl::GetServiceProperties(
    const dbus::ObjectPath& object_path) {
  return static_cast<ServiceProperties*>(
      object_manager_->GetProperties(object_path, peerd::kServiceInterface));
}

PeerDaemonManagerClient::PeerProperties*
PeerDaemonManagerClientImpl::GetPeerProperties(
    const dbus::ObjectPath& object_path) {
  return static_cast<PeerProperties*>(
      object_manager_->GetProperties(object_path, peerd::kPeerInterface));
}

void PeerDaemonManagerClientImpl::StartMonitoring(
    const std::vector<std::string>& requested_technologies,
    const base::DictionaryValue& options,
    const StringDBusMethodCallback& callback) {
  dbus::ObjectProxy* object_proxy = object_manager_->GetObjectProxy(
      dbus::ObjectPath(peerd::kPeerdManagerPath));
  if (!object_proxy) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&PeerDaemonManagerClientImpl::OnStringDBusMethod,
                   weak_ptr_factory_.GetWeakPtr(), callback, nullptr));
    return;
  }

  dbus::MethodCall method_call(peerd::kManagerInterface,
                               peerd::kStartMonitoringMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfStrings(requested_technologies);
  dbus::AppendValueData(&writer, options);
  object_proxy->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&PeerDaemonManagerClientImpl::OnStringDBusMethod,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void PeerDaemonManagerClientImpl::StopMonitoring(
    const std::string& monitoring_token,
    const VoidDBusMethodCallback& callback) {
  dbus::ObjectProxy* object_proxy = object_manager_->GetObjectProxy(
      dbus::ObjectPath(peerd::kPeerdManagerPath));
  if (!object_proxy) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&PeerDaemonManagerClientImpl::OnVoidDBusMethod,
                   weak_ptr_factory_.GetWeakPtr(), callback, nullptr));
    return;
  }

  dbus::MethodCall method_call(peerd::kManagerInterface,
                               peerd::kStopMonitoringMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(monitoring_token);
  object_proxy->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&PeerDaemonManagerClientImpl::OnVoidDBusMethod,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void PeerDaemonManagerClientImpl::ExposeService(
    const std::string& service_id,
    const std::map<std::string, std::string>& service_info,
    const base::DictionaryValue& options,
    const StringDBusMethodCallback& callback) {
  dbus::ObjectProxy* object_proxy = object_manager_->GetObjectProxy(
      dbus::ObjectPath(peerd::kPeerdManagerPath));
  if (!object_proxy) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&PeerDaemonManagerClientImpl::OnStringDBusMethod,
                   weak_ptr_factory_.GetWeakPtr(), callback, nullptr));
    return;
  }

  dbus::MethodCall method_call(peerd::kManagerInterface,
                               peerd::kExposeServiceMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(service_id);

  dbus::MessageWriter array_writer(nullptr);
  writer.OpenArray("{ss}", &array_writer);
  for (const auto& entry : service_info) {
    dbus::MessageWriter dict_entry_writer(nullptr);
    array_writer.OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString(entry.first);
    dict_entry_writer.AppendString(entry.second);
    array_writer.CloseContainer(&dict_entry_writer);
  }
  writer.CloseContainer(&array_writer);

  dbus::AppendValueData(&writer, options);
  object_proxy->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&PeerDaemonManagerClientImpl::OnStringDBusMethod,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void PeerDaemonManagerClientImpl::RemoveExposedService(
    const std::string& service_token,
    const VoidDBusMethodCallback& callback) {
  dbus::MethodCall method_call(peerd::kManagerInterface,
                               peerd::kRemoveExposedServiceMethod);
  dbus::ObjectProxy* object_proxy = object_manager_->GetObjectProxy(
      dbus::ObjectPath(peerd::kPeerdManagerPath));
  if (!object_proxy) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&PeerDaemonManagerClientImpl::OnVoidDBusMethod,
                   weak_ptr_factory_.GetWeakPtr(), callback, nullptr));
    return;
  }
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(service_token);
  object_proxy->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&PeerDaemonManagerClientImpl::OnVoidDBusMethod,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void PeerDaemonManagerClientImpl::Ping(
    const StringDBusMethodCallback& callback) {
  dbus::MethodCall method_call(peerd::kManagerInterface,
                               peerd::kPingMethod);
  dbus::ObjectProxy* object_proxy = object_manager_->GetObjectProxy(
      dbus::ObjectPath(peerd::kPeerdManagerPath));
  if (!object_proxy) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&PeerDaemonManagerClientImpl::OnStringDBusMethod,
                   weak_ptr_factory_.GetWeakPtr(), callback, nullptr));
    return;
  }
  dbus::MessageWriter writer(&method_call);
  object_proxy->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&PeerDaemonManagerClientImpl::OnStringDBusMethod,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

dbus::PropertySet* PeerDaemonManagerClientImpl::CreateProperties(
    dbus::ObjectProxy* object_proxy,
    const dbus::ObjectPath& object_path,
    const std::string& interface_name) {
  dbus::PropertySet* properties = nullptr;
  if (interface_name == peerd::kManagerInterface) {
    properties = new ManagerProperties(
        object_proxy,
        base::Bind(&PeerDaemonManagerClientImpl::OnManagerPropertyChanged,
                   weak_ptr_factory_.GetWeakPtr()));
  } else if (interface_name == peerd::kServiceInterface) {
    properties = new ServiceProperties(
        object_proxy,
        base::Bind(&PeerDaemonManagerClientImpl::OnServicePropertyChanged,
                   weak_ptr_factory_.GetWeakPtr(), object_path));
  } else if (interface_name == peerd::kPeerInterface) {
    properties = new PeerProperties(
        object_proxy,
        base::Bind(&PeerDaemonManagerClientImpl::OnPeerPropertyChanged,
                   weak_ptr_factory_.GetWeakPtr(), object_path));
  } else {
    NOTREACHED() << "Unhandled interface name " << interface_name;
  }
  return properties;
}

void PeerDaemonManagerClientImpl::ObjectAdded(
    const dbus::ObjectPath& object_path,
    const std::string& interface_name) {
  if (interface_name == peerd::kManagerInterface) {
    FOR_EACH_OBSERVER(Observer, observers_, ManagerAdded());
  } else if (interface_name == peerd::kServiceInterface) {
    FOR_EACH_OBSERVER(Observer, observers_, ServiceAdded(object_path));
  } else if (interface_name == peerd::kPeerInterface) {
    FOR_EACH_OBSERVER(Observer, observers_, PeerAdded(object_path));
  } else {
    NOTREACHED() << "Unhandled interface name " << interface_name;
  }
}

void PeerDaemonManagerClientImpl::ObjectRemoved(
    const dbus::ObjectPath& object_path,
    const std::string& interface_name) {
  if (interface_name == peerd::kManagerInterface) {
    FOR_EACH_OBSERVER(Observer, observers_, ManagerRemoved());
  } else if (interface_name == peerd::kServiceInterface) {
    FOR_EACH_OBSERVER(Observer, observers_, ServiceRemoved(object_path));
  } else if (interface_name == peerd::kPeerInterface) {
    FOR_EACH_OBSERVER(Observer, observers_, PeerRemoved(object_path));
  } else {
    NOTREACHED() << "Unhandled interface name " << interface_name;
  }
}

void PeerDaemonManagerClientImpl::OnStringDBusMethod(
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

void PeerDaemonManagerClientImpl::OnVoidDBusMethod(
    const VoidDBusMethodCallback& callback,
    dbus::Response* response) {
  callback.Run(response ? DBUS_METHOD_CALL_SUCCESS : DBUS_METHOD_CALL_FAILURE);
}

void PeerDaemonManagerClientImpl::Init(dbus::Bus* bus) {
  object_manager_ = bus->GetObjectManager(
      peerd::kPeerdServiceName,
      dbus::ObjectPath(peerd::kPeerdObjectManagerServicePath));
  object_manager_->RegisterInterface(peerd::kManagerInterface, this);
  object_manager_->RegisterInterface(peerd::kServiceInterface, this);
  object_manager_->RegisterInterface(peerd::kPeerInterface, this);
}

void PeerDaemonManagerClientImpl::OnManagerPropertyChanged(
    const std::string& property_name) {
  FOR_EACH_OBSERVER(Observer, observers_,
                    ManagerPropertyChanged(property_name));
}

void PeerDaemonManagerClientImpl::OnServicePropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  FOR_EACH_OBSERVER(Observer, observers_,
                    ServicePropertyChanged(object_path, property_name));
}

void PeerDaemonManagerClientImpl::OnPeerPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  FOR_EACH_OBSERVER(Observer, observers_,
                    PeerPropertyChanged(object_path, property_name));
}

}  // namespace

PeerDaemonManagerClient::ManagerProperties::ManagerProperties(
    dbus::ObjectProxy* object_proxy,
    const PropertyChangedCallback& callback)
    : dbus::PropertySet{object_proxy, peerd::kManagerInterface, callback} {
  RegisterProperty("MonitoredTechnologies", &monitored_technologies_);
}

PeerDaemonManagerClient::ManagerProperties::~ManagerProperties() {
}

PeerDaemonManagerClient::ServiceProperties::ServiceProperties(
    dbus::ObjectProxy* object_proxy,
    const PropertyChangedCallback& callback)
    : dbus::PropertySet{object_proxy, peerd::kServiceInterface, callback} {
  RegisterProperty("ServiceId", &service_id_);
  RegisterProperty("ServiceInfo", &service_info_);
  RegisterProperty("IpInfos", &ip_infos_);
}

PeerDaemonManagerClient::ServiceProperties::~ServiceProperties() {
}

PeerDaemonManagerClient::PeerProperties::PeerProperties(
    dbus::ObjectProxy* object_proxy,
    const PropertyChangedCallback& callback)
    : dbus::PropertySet{object_proxy, peerd::kPeerInterface, callback} {
  RegisterProperty("UUID", &uuid_);
  RegisterProperty("LastSeen", &last_seen_);
}

PeerDaemonManagerClient::PeerProperties::~PeerProperties() {
}

PeerDaemonManagerClient::PeerDaemonManagerClient() {
}

PeerDaemonManagerClient::~PeerDaemonManagerClient() {
}

// static
PeerDaemonManagerClient* PeerDaemonManagerClient::Create() {
  return new PeerDaemonManagerClientImpl();
}

}  // namespace chromeos
