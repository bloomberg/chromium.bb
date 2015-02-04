// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/privet_daemon_client.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_manager.h"
#include "dbus/object_proxy.h"
#include "dbus/property.h"

namespace chromeos {
namespace {

// The expected response to the Ping D-Bus method.
const char kPingResponse[] = "Hello world!";

// The PrivetDaemonClient implementation used in production. Note that privetd
// is not available on all devices. If privetd is not running this object
// returns empty property values and logs errors for attempted method calls but
// will not crash.
class PrivetDaemonClientImpl : public PrivetDaemonClient,
                               public dbus::ObjectManager::Interface {
 public:
  // Setup state properties. The object manager reads the initial values.
  struct Properties : public dbus::PropertySet {
    // Network bootstrap state. Read-only.
    dbus::Property<std::string> wifi_bootstrap_state;

    Properties(dbus::ObjectProxy* object_proxy,
               const std::string& interface_name,
               const PropertyChangedCallback& callback);
    ~Properties() override;
  };

  PrivetDaemonClientImpl();
  ~PrivetDaemonClientImpl() override;

  // DBusClient overrides:
  void Init(dbus::Bus* bus) override;

  // PrivetDaemonClient overrides:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  std::string GetWifiBootstrapState() override;
  void Ping(const PingCallback& callback) override;

 private:
  // dbus::ObjectManager::Interface overrides:
  dbus::PropertySet* CreateProperties(
      dbus::ObjectProxy* object_proxy,
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override;

  // Called with the owner of the privetd service at startup and again if the
  // owner changes.
  void OnServiceOwnerChanged(const std::string& service_owner);

  // Cleans up |object_manager_|.
  void CleanUpObjectManager();

  // Returns the instance of the property set or null if privetd is not running.
  Properties* GetProperties();

  // Called by dbus::PropertySet when a property value is changed, either by
  // result of a signal or response to a GetAll() or Get() call. Informs
  // observers.
  void OnPropertyChanged(const std::string& property_name);

  // Callback for the Ping D-Bus method.
  void OnPing(const PingCallback& callback, dbus::Response* response);

  // The current bus, usually the system bus. Not owned.
  dbus::Bus* bus_;

  // Object manager used to read D-Bus properties. Null if privetd is not
  // running. Not owned.
  dbus::ObjectManager* object_manager_;

  // Callback that monitors for service owner changes.
  dbus::Bus::GetServiceOwnerCallback service_owner_callback_;

  // The current service owner or the empty string is privetd is not running.
  std::string service_owner_;

  ObserverList<Observer> observers_;
  base::WeakPtrFactory<PrivetDaemonClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrivetDaemonClientImpl);
};

///////////////////////////////////////////////////////////////////////////////

PrivetDaemonClientImpl::Properties::Properties(
    dbus::ObjectProxy* object_proxy,
    const std::string& interface_name,
    const PropertyChangedCallback& callback)
    : dbus::PropertySet(object_proxy, interface_name, callback) {
  RegisterProperty(privetd::kWiFiBootstrapStateProperty, &wifi_bootstrap_state);
}

PrivetDaemonClientImpl::Properties::~Properties() {
}

///////////////////////////////////////////////////////////////////////////////

PrivetDaemonClientImpl::PrivetDaemonClientImpl()
    : bus_(nullptr), object_manager_(nullptr), weak_ptr_factory_(this) {
}

PrivetDaemonClientImpl::~PrivetDaemonClientImpl() {
  CleanUpObjectManager();
  if (bus_) {
    bus_->UnlistenForServiceOwnerChange(privetd::kPrivetdServiceName,
                                        service_owner_callback_);
  }
}

void PrivetDaemonClientImpl::Init(dbus::Bus* bus) {
  bus_ = bus;
  // privetd may not be running. Watch for it starting up later.
  service_owner_callback_ =
      base::Bind(&PrivetDaemonClientImpl::OnServiceOwnerChanged,
                 weak_ptr_factory_.GetWeakPtr());
  bus_->ListenForServiceOwnerChange(privetd::kPrivetdServiceName,
                                    service_owner_callback_);
  // Also explicitly check for its existence on startup.
  bus_->GetServiceOwner(privetd::kPrivetdServiceName, service_owner_callback_);
}

void PrivetDaemonClientImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PrivetDaemonClientImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::string PrivetDaemonClientImpl::GetWifiBootstrapState() {
  Properties* properties = GetProperties();
  if (!properties)
    return "";
  return properties->wifi_bootstrap_state.value();
}

dbus::PropertySet* PrivetDaemonClientImpl::CreateProperties(
    dbus::ObjectProxy* object_proxy,
    const dbus::ObjectPath& object_path,
    const std::string& interface_name) {
  return new Properties(object_proxy, interface_name,
                        base::Bind(&PrivetDaemonClientImpl::OnPropertyChanged,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void PrivetDaemonClientImpl::Ping(const PingCallback& callback) {
  if (!object_manager_) {
    LOG(ERROR) << "privetd not available.";
    return;
  }
  dbus::ObjectProxy* proxy = object_manager_->GetObjectProxy(
      dbus::ObjectPath(privetd::kPrivetdManagerServicePath));
  if (!proxy) {
    LOG(ERROR) << "Object not available.";
    return;
  }
  dbus::MethodCall method_call(privetd::kPrivetdManagerInterface,
                               privetd::kPingMethod);
  proxy->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                    base::Bind(&PrivetDaemonClientImpl::OnPing,
                               weak_ptr_factory_.GetWeakPtr(), callback));
}

void PrivetDaemonClientImpl::OnServiceOwnerChanged(
    const std::string& service_owner) {
  // If |service_owner| and |service_owner_| are both empty then the service
  // hasn't started yet. If they match the owner hasn't changed.
  if (service_owner == service_owner_)
    return;

  service_owner_ = service_owner;
  if (service_owner_.empty()) {
    LOG(ERROR) << "Lost privetd service.";
    CleanUpObjectManager();
    return;
  }

  // Finish initialization (or re-initialize if privetd restarted).
  object_manager_ =
      bus_->GetObjectManager(privetd::kPrivetdServiceName,
                             dbus::ObjectPath(privetd::kPrivetdServicePath));
  object_manager_->RegisterInterface(privetd::kPrivetdManagerInterface, this);
}

void PrivetDaemonClientImpl::CleanUpObjectManager() {
  if (object_manager_) {
    object_manager_->UnregisterInterface(privetd::kPrivetdManagerInterface);
    object_manager_ = nullptr;
  }
}

PrivetDaemonClientImpl::Properties* PrivetDaemonClientImpl::GetProperties() {
  if (!object_manager_)
    return nullptr;

  return static_cast<Properties*>(object_manager_->GetProperties(
      dbus::ObjectPath(privetd::kPrivetdManagerServicePath),
      privetd::kPrivetdManagerInterface));
}

void PrivetDaemonClientImpl::OnPropertyChanged(
    const std::string& property_name) {
  FOR_EACH_OBSERVER(PrivetDaemonClient::Observer, observers_,
                    OnPrivetDaemonPropertyChanged(property_name));
}

void PrivetDaemonClientImpl::OnPing(const PingCallback& callback,
                                    dbus::Response* response) {
  if (!response) {
    LOG(ERROR) << "Error calling " << privetd::kPingMethod;
    callback.Run(false);
    return;
  }
  dbus::MessageReader reader(response);
  std::string value;
  if (!reader.PopString(&value)) {
    LOG(ERROR) << "Error reading response from privetd: "
               << response->ToString();
  }
  // Returns success if the expected value is received.
  callback.Run(value == kPingResponse);
}

}  // namespace

//////////////////////////////////////////////////////////////////////////////

PrivetDaemonClient::PrivetDaemonClient() {
}

PrivetDaemonClient::~PrivetDaemonClient() {
}

// static
PrivetDaemonClient* PrivetDaemonClient::Create() {
  return new PrivetDaemonClientImpl();
}

}  // namespace chromeos
