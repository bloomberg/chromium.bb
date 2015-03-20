// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/privet_daemon_manager_client.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/observer_list.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_manager.h"
#include "dbus/object_proxy.h"

namespace chromeos {

namespace {

// TODO(benchan): Move these constants to system_api.
const char kManagerInterfaceName[] = "org.chromium.privetd.Manager";
const char kPrivetdManagerPath[] = "/org/chromium/privetd/Manager";
const char kPrivetdServiceName[] = "org.chromium.privetd";
const char kPrivetdServicePath[] = "/org/chromium/privetd";
const char kSetDescriptionMethod[] = "SetDescription";

const char kDescriptionProperty[] = "Description";
const char kGCDBootstrapStateProperty[] = "GCDBootstrapState";
const char kNameProperty[] = "Name";
const char kPairingInfoCodeProperty[] = "code";
const char kPairingInfoModeProperty[] = "mode";
const char kPairingInfoProperty[] = "PairingInfo";
const char kPairingInfoSessionIdProperty[] = "sessionId";
const char kWiFiBootstrapStateProperty[] = "WiFiBootstrapState";

// The PrivetDaemonManagerClient implementation used in production.
class PrivetDaemonManagerClientImpl : public PrivetDaemonManagerClient,
                                      public dbus::ObjectManager::Interface {
 public:
  PrivetDaemonManagerClientImpl();
  ~PrivetDaemonManagerClientImpl() override;

  // PrivetDaemonManagerClient overrides.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void SetDescription(const std::string& description,
                      const VoidDBusMethodCallback& callback) override;
  const Properties* GetProperties() override;

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
  void OnManagerPropertyChanged(const std::string& property_name);
  void OnVoidDBusMethod(const VoidDBusMethodCallback& callback,
                        dbus::Response* response);

  // List of observers interested in event notifications from us.
  ObserverList<Observer> observers_;
  dbus::ObjectManager* object_manager_;
  base::WeakPtrFactory<PrivetDaemonManagerClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrivetDaemonManagerClientImpl);
};

PrivetDaemonManagerClientImpl::PrivetDaemonManagerClientImpl()
    : object_manager_(nullptr), weak_ptr_factory_(this) {
}

PrivetDaemonManagerClientImpl::~PrivetDaemonManagerClientImpl() {
  if (object_manager_) {
    object_manager_->UnregisterInterface(kManagerInterfaceName);
  }
}

void PrivetDaemonManagerClientImpl::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void PrivetDaemonManagerClientImpl::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void PrivetDaemonManagerClientImpl::SetDescription(
    const std::string& description,
    const VoidDBusMethodCallback& callback) {
  dbus::ObjectProxy* object_proxy =
      object_manager_->GetObjectProxy(dbus::ObjectPath(kPrivetdManagerPath));
  if (!object_proxy) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&PrivetDaemonManagerClientImpl::OnVoidDBusMethod,
                   weak_ptr_factory_.GetWeakPtr(), callback, nullptr));
    return;
  }

  dbus::MethodCall method_call(kManagerInterfaceName, kSetDescriptionMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(description);
  object_proxy->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&PrivetDaemonManagerClientImpl::OnVoidDBusMethod,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

const PrivetDaemonManagerClient::Properties*
PrivetDaemonManagerClientImpl::GetProperties() {
  return static_cast<Properties*>(object_manager_->GetProperties(
      dbus::ObjectPath(kPrivetdManagerPath), kManagerInterfaceName));
}

void PrivetDaemonManagerClientImpl::Init(dbus::Bus* bus) {
  object_manager_ = bus->GetObjectManager(
      kPrivetdServiceName, dbus::ObjectPath(kPrivetdServicePath));
  object_manager_->RegisterInterface(kManagerInterfaceName, this);
}

dbus::PropertySet* PrivetDaemonManagerClientImpl::CreateProperties(
    dbus::ObjectProxy* object_proxy,
    const dbus::ObjectPath& object_path,
    const std::string& interface_name) {
  dbus::PropertySet* properties = nullptr;
  if (interface_name == kManagerInterfaceName) {
    properties = new Properties(
        object_proxy, interface_name,
        base::Bind(&PrivetDaemonManagerClientImpl::OnManagerPropertyChanged,
                   weak_ptr_factory_.GetWeakPtr()));
  } else {
    NOTREACHED() << "Unhandled interface name " << interface_name;
  }
  return properties;
}

void PrivetDaemonManagerClientImpl::ObjectAdded(
    const dbus::ObjectPath& object_path,
    const std::string& interface_name) {
  if (interface_name == kManagerInterfaceName) {
    FOR_EACH_OBSERVER(Observer, observers_, ManagerAdded());
  } else {
    NOTREACHED() << "Unhandled interface name " << interface_name;
  }
}

void PrivetDaemonManagerClientImpl::ObjectRemoved(
    const dbus::ObjectPath& object_path,
    const std::string& interface_name) {
  if (interface_name == kManagerInterfaceName) {
    FOR_EACH_OBSERVER(Observer, observers_, ManagerRemoved());
  } else {
    NOTREACHED() << "Unhandled interface name " << interface_name;
  }
}

void PrivetDaemonManagerClientImpl::OnManagerPropertyChanged(
    const std::string& property_name) {
  FOR_EACH_OBSERVER(Observer, observers_,
                    ManagerPropertyChanged(property_name));
}

void PrivetDaemonManagerClientImpl::OnVoidDBusMethod(
    const VoidDBusMethodCallback& callback,
    dbus::Response* response) {
  callback.Run(response ? DBUS_METHOD_CALL_SUCCESS : DBUS_METHOD_CALL_FAILURE);
}

}  // namespace

void PrivetDaemonManagerClient::PairingInfo::Clear() {
  code_.clear();
  mode_.clear();
  session_id_.clear();
}

bool PrivetDaemonManagerClient::PairingInfoProperty::PopValueFromReader(
    dbus::MessageReader* reader) {
  dbus::MessageReader variant_reader(nullptr);
  dbus::MessageReader array_reader(nullptr);
  value_.Clear();
  if (!reader->PopVariant(&variant_reader) ||
      !variant_reader.PopArray(&array_reader)) {
    return false;
  }
  while (array_reader.HasMoreData()) {
    dbus::MessageReader dict_entry_reader(nullptr);
    if (!array_reader.PopDictEntry(&dict_entry_reader))
      return false;
    std::string key;
    if (!dict_entry_reader.PopString(&key))
      return false;

    dbus::MessageReader field_reader(nullptr);
    if (!dict_entry_reader.PopVariant(&field_reader))
      return false;

    if (field_reader.GetDataSignature() == "s") {
      std::string string_value;
      if (!field_reader.PopString(&string_value))
        return false;
      if (key == kPairingInfoSessionIdProperty) {
        value_.set_session_id(string_value);
      } else if (key == kPairingInfoModeProperty) {
        value_.set_mode(string_value);
      }
    } else if (field_reader.GetDataSignature() == "ay") {
      const uint8* bytes = nullptr;
      size_t length = 0;
      if (!field_reader.PopArrayOfBytes(&bytes, &length))
        return false;
      if (key == kPairingInfoCodeProperty)
        value_.set_code(bytes, length);
    }
  }
  return true;
}

PrivetDaemonManagerClient::PairingInfo::PairingInfo() {
}

PrivetDaemonManagerClient::PairingInfo::~PairingInfo() {
}

void PrivetDaemonManagerClient::PairingInfoProperty::AppendSetValueToWriter(
    dbus::MessageWriter* writer) {
  NOTIMPLEMENTED();
}

void PrivetDaemonManagerClient::PairingInfoProperty::
    ReplaceValueWithSetValue() {
  NOTIMPLEMENTED();
}

PrivetDaemonManagerClient::Properties::Properties(
    dbus::ObjectProxy* object_proxy,
    const std::string& interface_name,
    const PropertyChangedCallback& callback)
    : dbus::PropertySet(object_proxy, interface_name, callback) {
  RegisterProperty(kWiFiBootstrapStateProperty, &wifi_bootstrap_state_);
  RegisterProperty(kGCDBootstrapStateProperty, &gcd_bootstrap_state_);
  RegisterProperty(kPairingInfoProperty, &pairing_info_);
  RegisterProperty(kDescriptionProperty, &description_);
  RegisterProperty(kNameProperty, &name_);
}

PrivetDaemonManagerClient::Properties::~Properties() {
}

PrivetDaemonManagerClient::Observer::~Observer() {
}

PrivetDaemonManagerClient::PrivetDaemonManagerClient() {
}

PrivetDaemonManagerClient::~PrivetDaemonManagerClient() {
}

// static
PrivetDaemonManagerClient* PrivetDaemonManagerClient::Create() {
  return new PrivetDaemonManagerClientImpl();
}

}  // namespace chromeos
