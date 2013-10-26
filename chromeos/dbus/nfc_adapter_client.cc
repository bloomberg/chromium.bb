// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/nfc_adapter_client.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/strings/stringprintf.h"
#include "chromeos/dbus/fake_nfc_adapter_client.h"
#include "chromeos/dbus/nfc_manager_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

NfcAdapterClient::Properties::Properties(
    dbus::ObjectProxy* object_proxy,
    const PropertyChangedCallback& callback)
    : NfcPropertySet(object_proxy,
                     nfc_adapter::kNfcAdapterInterface,
                     callback) {
  RegisterProperty(nfc_adapter::kModeProperty, &mode);
  RegisterProperty(nfc_adapter::kPoweredProperty, &powered);
  RegisterProperty(nfc_adapter::kPollingProperty, &polling);
  RegisterProperty(nfc_adapter::kProtocolsProperty, &protocols);
  RegisterProperty(nfc_adapter::kTagsProperty, &tags);
  RegisterProperty(nfc_adapter::kDevicesProperty, &devices);
}

NfcAdapterClient::Properties::~Properties() {
}

// The NfcAdapterClient implementation used in production.
class NfcAdapterClientImpl
    : public NfcAdapterClient,
      public NfcManagerClient::Observer,
      public nfc_client_helpers::DBusObjectMap::Delegate {
 public:
  explicit NfcAdapterClientImpl(NfcManagerClient* manager_client)
      : initial_adapters_received_(false),
        bus_(NULL),
        manager_client_(manager_client),
        weak_ptr_factory_(this) {
    DCHECK(manager_client);
  }

  virtual ~NfcAdapterClientImpl() {
    manager_client_->RemoveObserver(this);
  }

  // NfcAdapterClient override.
  virtual void AddObserver(NfcAdapterClient::Observer* observer) OVERRIDE {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  // NfcAdapterClient override.
  virtual void RemoveObserver(NfcAdapterClient::Observer* observer) OVERRIDE {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  // NfcAdapterClient override.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path)
      OVERRIDE {
    return static_cast<Properties*>(
        object_map_->GetObjectProperties(object_path));
  }

  // NfcAdapterClient override.
  virtual void StartPollLoop(
      const dbus::ObjectPath& object_path,
      const std::string& mode,
      const base::Closure& callback,
      const nfc_client_helpers::ErrorCallback& error_callback) OVERRIDE {
    dbus::ObjectProxy* object_proxy = object_map_->GetObjectProxy(object_path);
    if (!object_proxy) {
      std::string error_message =
          base::StringPrintf("NFC adapter with object path \"%s\" does not "
                             "exist.", object_path.value().c_str());
      LOG(ERROR) << error_message;
      error_callback.Run(nfc_client_helpers::kUnknownObjectError,
                         error_message);
      return;
    }
    dbus::MethodCall method_call(nfc_adapter::kNfcAdapterInterface,
                                 nfc_adapter::kStartPollLoop);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(mode);
    object_proxy->CallMethodWithErrorCallback(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&nfc_client_helpers::OnSuccess, callback),
        base::Bind(&nfc_client_helpers::OnError, error_callback));
  }

  // NfcAdapterClient override.
  virtual void StopPollLoop(
      const dbus::ObjectPath& object_path,
      const base::Closure& callback,
      const nfc_client_helpers::ErrorCallback& error_callback) OVERRIDE {
    dbus::ObjectProxy* object_proxy = object_map_->GetObjectProxy(object_path);
    if (!object_proxy) {
      std::string error_message =
          base::StringPrintf("NFC adapter with object path \"%s\" does not "
                             "exist.", object_path.value().c_str());
      LOG(ERROR) << error_message;
      error_callback.Run(nfc_client_helpers::kUnknownObjectError,
                         error_message);
      return;
    }
    dbus::MethodCall method_call(nfc_adapter::kNfcAdapterInterface,
                                 nfc_adapter::kStopPollLoop);
    object_proxy->CallMethodWithErrorCallback(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&nfc_client_helpers::OnSuccess, callback),
        base::Bind(&nfc_client_helpers::OnError, error_callback));
  }

 protected:
  // DBusClient override.
  virtual void Init(dbus::Bus* bus) OVERRIDE {
    VLOG(1) << "Creating NfcAdapterClientImpl";
    DCHECK(bus);
    bus_ = bus;
    object_map_.reset(new nfc_client_helpers::DBusObjectMap(
        nfc_adapter::kNfcAdapterServiceName, this, bus));
    DCHECK(manager_client_);
    manager_client_->AddObserver(this);
  }

 private:
  // NfcManagerClient::Observer override.
  virtual void AdapterAdded(const dbus::ObjectPath& object_path) OVERRIDE {
    VLOG(1) << "AdapterAdded: " << object_path.value();
    // Initialize the object proxy here, so that observers can start receiving
    // notifications for it and it is cached for reuse. Note that, even if we
    // miss this signal, a proxy will be created on demand for any object paths
    // that are passed to the public methods later.
    object_map_->AddObject(object_path);
    FOR_EACH_OBSERVER(NfcAdapterClient::Observer, observers_,
                      AdapterAdded(object_path));
  }

  // NfcManagerClient::Observer override.
  virtual void AdapterRemoved(const dbus::ObjectPath& object_path) OVERRIDE {
    VLOG(1) << "AdapterRemoved: " << object_path.value();
    // Remove the object proxy, as we know that the adapter no longer exists.
    // Note that this doesn't prevent a client from recreating a proxy for an
    // object path that is no longer valid, as proxies are created on demand as
    // necessary by the public methods.
    object_map_->RemoveObject(object_path);
    FOR_EACH_OBSERVER(NfcAdapterClient::Observer, observers_,
                      AdapterRemoved(object_path));
  }

  // NfcManagerClient::Observer override.
  virtual void ManagerPropertyChanged(const std::string& property_name)
        OVERRIDE {
    NfcManagerClient::Properties* manager_properties =
        manager_client_->GetProperties();
    if (!initial_adapters_received_ &&
        property_name == manager_properties->adapters.name()) {
      initial_adapters_received_ = true;
      VLOG(1) << "Initial set of adapters received.";
      // Create proxies for all adapters that are known to the manager, so that
      // observers can start getting notified for any signals emitted by
      // adapters. We use the PropertyChanged signal from manager only to
      // create proxies for the initial fetch. We rely on the AdapterAdded and
      // AdapterRemoved signals after that.
      std::vector<dbus::ObjectPath> adapters =
          manager_properties->adapters.value();
      for (std::vector<dbus::ObjectPath>::iterator iter = adapters.begin();
           iter != adapters.end(); ++iter) {
        VLOG(1) << "Creating proxy for: " << iter->value();
        object_map_->AddObject(*iter);
      }
    }
  }

  // nfc_client_helpers::DBusObjectMap::Delegate override.
  virtual NfcPropertySet* CreateProperties(
      dbus::ObjectProxy* object_proxy) OVERRIDE {
    return new Properties(
        object_proxy,
        base::Bind(&NfcAdapterClientImpl::OnPropertyChanged,
                   weak_ptr_factory_.GetWeakPtr(),
                   object_proxy->object_path()));
  }

  // Called by NfcPropertySet when a property value is changed, either by
  // result of a signal or response to a GetAll() or Get() call.
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name) {
    VLOG(1) << "Adapter property changed; Path: " << object_path.value()
            << " Property: " << property_name;
    FOR_EACH_OBSERVER(NfcAdapterClient::Observer, observers_,
                      AdapterPropertyChanged(object_path, property_name));
  }

  // This variable stores whether or not we have ever been notified of
  // ManagerPropertiesChanged. This is used to bootstrap adapter proxies
  // after receiving the initial set of properties from the NFC manager.
  bool initial_adapters_received_;

  // We maintain a pointer to the bus to be able to request proxies for
  // new NFC adapters that appear.
  dbus::Bus* bus_;

  // Mapping from object paths to object proxies and properties structures that
  // were already created by us.
  scoped_ptr<nfc_client_helpers::DBusObjectMap> object_map_;

  // The manager client that we listen to events notifications from.
  NfcManagerClient* manager_client_;

  // List of observers interested in event notifications.
  ObserverList<NfcAdapterClient::Observer> observers_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<NfcAdapterClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NfcAdapterClientImpl);
};

NfcAdapterClient::NfcAdapterClient() {
}

NfcAdapterClient::~NfcAdapterClient() {
}

NfcAdapterClient* NfcAdapterClient::Create(DBusClientImplementationType type,
                                           NfcManagerClient* manager_client) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new NfcAdapterClientImpl(manager_client);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new FakeNfcAdapterClient();
}

}  // namespace chromeos
