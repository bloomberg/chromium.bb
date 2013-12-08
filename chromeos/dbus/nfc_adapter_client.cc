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
      : bus_(NULL),
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
  virtual std::vector<dbus::ObjectPath> GetAdapters() OVERRIDE {
    return object_map_->GetObjectPaths();
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
  virtual void ManagerPropertyChanged(
      const std::string& property_name) OVERRIDE {
    // Update the adapter proxies.
    DCHECK(manager_client_);
    NfcManagerClient::Properties* manager_properties =
        manager_client_->GetProperties();

    // Ignore changes to properties other than "Adapters".
    if (property_name != manager_properties->adapters.name())
      return;

    // Update the known adapters.
    VLOG(1) << "NFC adapters changed.";
    const std::vector<dbus::ObjectPath>& received_adapters =
        manager_properties->adapters.value();
    object_map_->UpdateObjects(received_adapters);
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

  // nfc_client_helpers::DBusObjectMap::Delegate override.
  virtual void ObjectAdded(const dbus::ObjectPath& object_path) OVERRIDE {
    FOR_EACH_OBSERVER(NfcAdapterClient::Observer, observers_,
                      AdapterAdded(object_path));
  }

  // nfc_client_helpers::DBusObjectMap::Delegate override.
  virtual void ObjectRemoved(const dbus::ObjectPath& object_path) OVERRIDE {
    FOR_EACH_OBSERVER(NfcAdapterClient::Observer, observers_,
                      AdapterRemoved(object_path));
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

  // We maintain a pointer to the bus to be able to request proxies for
  // new NFC adapters that appear.
  dbus::Bus* bus_;

  // List of observers interested in event notifications.
  ObserverList<NfcAdapterClient::Observer> observers_;

  // Mapping from object paths to object proxies and properties structures that
  // were already created by us.
  scoped_ptr<nfc_client_helpers::DBusObjectMap> object_map_;

  // The manager client that we listen to events notifications from.
  NfcManagerClient* manager_client_;

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

NfcAdapterClient* NfcAdapterClient::Create(NfcManagerClient* manager_client) {
  return new NfcAdapterClientImpl(manager_client);
}

}  // namespace chromeos
