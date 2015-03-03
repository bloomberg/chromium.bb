// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/leadership_daemon_manager_client.h"

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
namespace leaderd {
const char kLeaderdServiceName[] = "org.chromium.leaderd";
const char kLeaderdObjectManagerServicePath[] = "/org/chromium/leaderd";
const char kLeaderdManagerPath[] = "/org/chromium/leaderd/Manager";
const char kManagerInterface[] = "org.chromium.leaderd.Manager";
const char kGroupInterface[] = "org.chromium.leaderd.Group";
const char kJoinGroupMethod[] = "JoinGroup";
const char kLeaveGroupMethod[] = "LeaveGroup";
const char kSetScoreMethod[] = "SetScore";
const char kPokeLeaderMethod[] = "PokeLeader";
const char kPingMethod[] = "Ping";
const char kLeaderUUID[] = "LeaderUUID";
const char kGroupMembers[] = "GroupMembers";
}  // namespace leaderd

namespace {

// Since there is no property associated with Manager objects, an empty callback
// is used.
void DoNothing(const std::string& property_name) {
}

// The LeadershipDaemonManagerClient implementation used in production.
class LeadershipDaemonManagerClientImpl
    : public LeadershipDaemonManagerClient,
      public dbus::ObjectManager::Interface {
 public:
  LeadershipDaemonManagerClientImpl();
  ~LeadershipDaemonManagerClientImpl() override;

  // LeadershipDaemonManagerClient overrides.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void JoinGroup(const std::string& group,
                 const base::DictionaryValue& options,
                 const ObjectPathDBusMethodCallback& callback) override;
  void LeaveGroup(const std::string& object_path,
                  const VoidDBusMethodCallback& callback) override;
  void SetScore(const std::string& object_path,
                int score,
                const VoidDBusMethodCallback& callback) override;
  void PokeLeader(const std::string& object_path,
                  const VoidDBusMethodCallback& callback) override;
  void Ping(const StringDBusMethodCallback& callback) override;
  const GroupProperties* GetGroupProperties(
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
  void OnGroupPropertyChanged(const dbus::ObjectPath& object_path,
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
  base::WeakPtrFactory<LeadershipDaemonManagerClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LeadershipDaemonManagerClientImpl);
};

LeadershipDaemonManagerClientImpl::LeadershipDaemonManagerClientImpl()
    : object_manager_(nullptr), weak_ptr_factory_(this) {
}

LeadershipDaemonManagerClientImpl::~LeadershipDaemonManagerClientImpl() {
  if (object_manager_) {
    object_manager_->UnregisterInterface(leaderd::kManagerInterface);
    object_manager_->UnregisterInterface(leaderd::kGroupInterface);
  }
}

void LeadershipDaemonManagerClientImpl::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void LeadershipDaemonManagerClientImpl::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void LeadershipDaemonManagerClientImpl::JoinGroup(
    const std::string& group,
    const base::DictionaryValue& options,
    const ObjectPathDBusMethodCallback& callback) {
  dbus::ObjectProxy* object_proxy = object_manager_->GetObjectProxy(
      dbus::ObjectPath(leaderd::kLeaderdManagerPath));
  if (!object_proxy) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&LeadershipDaemonManagerClientImpl::OnObjectPathDBusMethod,
                   weak_ptr_factory_.GetWeakPtr(), callback, nullptr));
    return;
  }

  dbus::MethodCall method_call(leaderd::kManagerInterface,
                               leaderd::kJoinGroupMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(group);
  dbus::AppendValueData(&writer, options);
  object_proxy->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&LeadershipDaemonManagerClientImpl::OnObjectPathDBusMethod,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void LeadershipDaemonManagerClientImpl::LeaveGroup(
    const std::string& object_path,
    const VoidDBusMethodCallback& callback) {
  dbus::ObjectProxy* object_proxy =
      object_manager_->GetObjectProxy(dbus::ObjectPath(object_path));
  if (!object_proxy) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&LeadershipDaemonManagerClientImpl::OnVoidDBusMethod,
                   weak_ptr_factory_.GetWeakPtr(), callback, nullptr));
    return;
  }

  dbus::MethodCall method_call(leaderd::kGroupInterface,
                               leaderd::kLeaveGroupMethod);
  dbus::MessageWriter writer(&method_call);
  object_proxy->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&LeadershipDaemonManagerClientImpl::OnVoidDBusMethod,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void LeadershipDaemonManagerClientImpl::SetScore(
    const std::string& object_path,
    int score,
    const VoidDBusMethodCallback& callback) {
  dbus::ObjectProxy* object_proxy =
      object_manager_->GetObjectProxy(dbus::ObjectPath(object_path));
  if (!object_proxy) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&LeadershipDaemonManagerClientImpl::OnVoidDBusMethod,
                   weak_ptr_factory_.GetWeakPtr(), callback, nullptr));
    return;
  }

  dbus::MethodCall method_call(leaderd::kGroupInterface,
                               leaderd::kSetScoreMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendInt32(score);
  object_proxy->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&LeadershipDaemonManagerClientImpl::OnVoidDBusMethod,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void LeadershipDaemonManagerClientImpl::PokeLeader(
    const std::string& object_path,
    const VoidDBusMethodCallback& callback) {
  dbus::ObjectProxy* object_proxy =
      object_manager_->GetObjectProxy(dbus::ObjectPath(object_path));
  if (!object_proxy) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&LeadershipDaemonManagerClientImpl::OnVoidDBusMethod,
                   weak_ptr_factory_.GetWeakPtr(), callback, nullptr));
    return;
  }

  dbus::MethodCall method_call(leaderd::kGroupInterface,
                               leaderd::kPokeLeaderMethod);
  dbus::MessageWriter writer(&method_call);
  object_proxy->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&LeadershipDaemonManagerClientImpl::OnVoidDBusMethod,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void LeadershipDaemonManagerClientImpl::Ping(
    const StringDBusMethodCallback& callback) {
  dbus::ObjectProxy* object_proxy = object_manager_->GetObjectProxy(
      dbus::ObjectPath(leaderd::kLeaderdManagerPath));
  if (!object_proxy) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&LeadershipDaemonManagerClientImpl::OnStringDBusMethod,
                   weak_ptr_factory_.GetWeakPtr(), callback, nullptr));
    return;
  }

  dbus::MethodCall method_call(leaderd::kManagerInterface,
                               leaderd::kPingMethod);
  dbus::MessageWriter writer(&method_call);
  object_proxy->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&LeadershipDaemonManagerClientImpl::OnStringDBusMethod,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

const LeadershipDaemonManagerClient::GroupProperties*
LeadershipDaemonManagerClientImpl::GetGroupProperties(
    const dbus::ObjectPath& object_path) {
  return static_cast<GroupProperties*>(
      object_manager_->GetProperties(object_path, leaderd::kGroupInterface));
}

void LeadershipDaemonManagerClientImpl::Init(dbus::Bus* bus) {
  object_manager_ = bus->GetObjectManager(
      leaderd::kLeaderdServiceName,
      dbus::ObjectPath(leaderd::kLeaderdObjectManagerServicePath));
  object_manager_->RegisterInterface(leaderd::kManagerInterface, this);
  object_manager_->RegisterInterface(leaderd::kGroupInterface, this);
}

dbus::PropertySet* LeadershipDaemonManagerClientImpl::CreateProperties(
    dbus::ObjectProxy* object_proxy,
    const dbus::ObjectPath& object_path,
    const std::string& interface_name) {
  dbus::PropertySet* properties = nullptr;
  if (interface_name == leaderd::kManagerInterface) {
    properties = new dbus::PropertySet(object_proxy, interface_name,
                                       base::Bind(&DoNothing));
  } else if (interface_name == leaderd::kGroupInterface) {
    properties = new GroupProperties(
        object_proxy, interface_name,
        base::Bind(&LeadershipDaemonManagerClientImpl::OnGroupPropertyChanged,
                   weak_ptr_factory_.GetWeakPtr(), object_path));
  } else {
    NOTREACHED() << "Unhandled interface name " << interface_name;
  }
  return properties;
}

void LeadershipDaemonManagerClientImpl::ObjectAdded(
    const dbus::ObjectPath& object_path,
    const std::string& interface_name) {
  if (interface_name == leaderd::kManagerInterface) {
    FOR_EACH_OBSERVER(Observer, observers_, ManagerAdded());
  } else if (interface_name == leaderd::kGroupInterface) {
    FOR_EACH_OBSERVER(Observer, observers_, GroupAdded(object_path));
  } else {
    NOTREACHED() << "Unhandled interface name " << interface_name;
  }
}

void LeadershipDaemonManagerClientImpl::ObjectRemoved(
    const dbus::ObjectPath& object_path,
    const std::string& interface_name) {
  if (interface_name == leaderd::kManagerInterface) {
    FOR_EACH_OBSERVER(Observer, observers_, ManagerRemoved());
  } else if (interface_name == leaderd::kGroupInterface) {
    FOR_EACH_OBSERVER(Observer, observers_, GroupRemoved(object_path));
  } else {
    NOTREACHED() << "Unhandled interface name " << interface_name;
  }
}

void LeadershipDaemonManagerClientImpl::OnGroupPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  FOR_EACH_OBSERVER(Observer, observers_,
                    GroupPropertyChanged(object_path, property_name));
}

void LeadershipDaemonManagerClientImpl::OnObjectPathDBusMethod(
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

void LeadershipDaemonManagerClientImpl::OnStringDBusMethod(
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

void LeadershipDaemonManagerClientImpl::OnVoidDBusMethod(
    const VoidDBusMethodCallback& callback,
    dbus::Response* response) {
  callback.Run(response ? DBUS_METHOD_CALL_SUCCESS : DBUS_METHOD_CALL_FAILURE);
}

}  // namespace

LeadershipDaemonManagerClient::GroupProperties::GroupProperties(
    dbus::ObjectProxy* object_proxy,
    const std::string& interface_name,
    const PropertyChangedCallback& callback)
    : dbus::PropertySet(object_proxy, interface_name, callback) {
  RegisterProperty(leaderd::kLeaderUUID, &leader_uuid_);
  RegisterProperty(leaderd::kGroupMembers, &group_members_);
}

LeadershipDaemonManagerClient::GroupProperties::~GroupProperties() {
}

LeadershipDaemonManagerClient::Observer::~Observer() {
}

void LeadershipDaemonManagerClient::Observer::ManagerAdded() {
}

void LeadershipDaemonManagerClient::Observer::ManagerRemoved() {
}

void LeadershipDaemonManagerClient::Observer::GroupAdded(
    const dbus::ObjectPath& object_path) {
}

void LeadershipDaemonManagerClient::Observer::GroupRemoved(
    const dbus::ObjectPath& object_path) {
}

void LeadershipDaemonManagerClient::Observer::GroupPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
}

LeadershipDaemonManagerClient::LeadershipDaemonManagerClient() {
}

LeadershipDaemonManagerClient::~LeadershipDaemonManagerClient() {
}

// static
LeadershipDaemonManagerClient* LeadershipDaemonManagerClient::Create() {
  return new LeadershipDaemonManagerClientImpl();
}

}  // namespace chromeos
