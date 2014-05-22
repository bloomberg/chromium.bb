// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/nfc_tag_client.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/stringprintf.h"
#include "chromeos/dbus/nfc_adapter_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/values_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using chromeos::nfc_client_helpers::DBusObjectMap;
using chromeos::nfc_client_helpers::ObjectProxyTree;

namespace chromeos {

NfcTagClient::Properties::Properties(
    dbus::ObjectProxy* object_proxy,
    const PropertyChangedCallback& callback)
    : NfcPropertySet(object_proxy,
                     nfc_tag::kNfcTagInterface,
                     callback) {
  RegisterProperty(nfc_tag::kTypeProperty, &type);
  RegisterProperty(nfc_tag::kProtocolProperty, &protocol);
  RegisterProperty(nfc_tag::kRecordsProperty, &records);
  RegisterProperty(nfc_tag::kReadOnlyProperty, &read_only);
}

NfcTagClient::Properties::~Properties() {
}

// The NfcTagClient implementation used in production.
class NfcTagClientImpl : public NfcTagClient,
                         public NfcAdapterClient::Observer,
                         public DBusObjectMap::Delegate {
 public:
  explicit NfcTagClientImpl(NfcAdapterClient* adapter_client)
      : bus_(NULL),
        adapter_client_(adapter_client),
        weak_ptr_factory_(this) {
    DCHECK(adapter_client);
  }

  virtual ~NfcTagClientImpl() {
    DCHECK(adapter_client_);
    adapter_client_->RemoveObserver(this);
  }

  // NfcTagClient override.
  virtual void AddObserver(NfcTagClient::Observer* observer) OVERRIDE {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  // NfcTagClient override.
  virtual void RemoveObserver(NfcTagClient::Observer* observer) OVERRIDE {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  // NfcTagClient override.
  virtual std::vector<dbus::ObjectPath> GetTagsForAdapter(
      const dbus::ObjectPath& adapter_path) OVERRIDE {
    DBusObjectMap* object_map =
        adapters_to_object_maps_.GetObjectMap(adapter_path);
    if (!object_map)
      return std::vector<dbus::ObjectPath>();
    return object_map->GetObjectPaths();
  }

  // NfcTagClient override.
  virtual Properties* GetProperties(
      const dbus::ObjectPath& object_path) OVERRIDE {
    return static_cast<Properties*>(
        adapters_to_object_maps_.FindObjectProperties(object_path));
  }

  // NfcTagClient override.
  virtual void Write(
      const dbus::ObjectPath& object_path,
      const base::DictionaryValue& attributes,
      const base::Closure& callback,
      const nfc_client_helpers::ErrorCallback& error_callback) OVERRIDE {
    dbus::ObjectProxy* object_proxy =
        adapters_to_object_maps_.FindObjectProxy(object_path);
    if (!object_proxy) {
      std::string error_message =
          base::StringPrintf("NFC tag with object path \"%s\" does not exist.",
                             object_path.value().c_str());
      LOG(ERROR) << error_message;
      error_callback.Run(nfc_client_helpers::kUnknownObjectError,
                         error_message);
      return;
    }

    // |attributes| should not be empty.
    if (attributes.empty()) {
      std::string error_message =
          "Cannot write data to tag with empty arguments.";
      LOG(ERROR) << error_message;
      error_callback.Run(nfc_error::kInvalidArguments, error_message);
      return;
    }

    // Create the arguments.
    dbus::MethodCall method_call(nfc_tag::kNfcTagInterface, nfc_tag::kWrite);
    dbus::MessageWriter writer(&method_call);
    dbus::AppendValueData(&writer, attributes);

    object_proxy->CallMethodWithErrorCallback(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&nfc_client_helpers::OnSuccess, callback),
        base::Bind(&nfc_client_helpers::OnError, error_callback));
  }

 protected:
  // DBusClient override.
  virtual void Init(dbus::Bus* bus) OVERRIDE {
    VLOG(1) << "Creating NfcTagClientImpl";
    DCHECK(bus);
    bus_ = bus;
    DCHECK(adapter_client_);
    adapter_client_->AddObserver(this);
  }

 private:
  // NfcAdapterClient::Observer override.
  virtual void AdapterAdded(const dbus::ObjectPath& object_path) OVERRIDE {
    VLOG(1) << "Adapter added. Creating map for tag proxies belonging to "
            << "adapter: " << object_path.value();
    adapters_to_object_maps_.CreateObjectMap(
        object_path, nfc_tag::kNfcTagServiceName, this, bus_);
  }

  // NfcAdapterClient::Observer override.
  virtual void AdapterRemoved(const dbus::ObjectPath& object_path) OVERRIDE {
    // Neard doesn't send out property changed signals for the tags that
    // are removed when the adapter they belong to is removed. Clean up the
    // object proxies for devices that are managed by the removed adapter.
    // Note: DBusObjectMap guarantees that the Properties structure for the
    // removed adapter will be valid before this method returns.
    VLOG(1) << "Adapter removed. Cleaning up tag proxies belonging to "
            << "adapter: " << object_path.value();
    adapters_to_object_maps_.RemoveObjectMap(object_path);
  }

  // NfcAdapterClient::Observer override.
  virtual void AdapterPropertyChanged(
      const dbus::ObjectPath& object_path,
      const std::string& property_name) OVERRIDE {
    // Update the tag proxies.
    DCHECK(adapter_client_);
    NfcAdapterClient::Properties *adapter_properties =
        adapter_client_->GetProperties(object_path);
    DCHECK(adapter_properties);
    if (!adapter_properties) {
      LOG(ERROR) << "No property structure found for adapter: "
                 << object_path.value();
      return;
    }

    // Ignore changes to properties other than "Tags".
    if (property_name != adapter_properties->tags.name())
      return;

    // Update the known tags.
    VLOG(1) << "NFC tags changed.";
    const std::vector<dbus::ObjectPath>& received_tags =
        adapter_properties->tags.value();
    DBusObjectMap* object_map =
        adapters_to_object_maps_.GetObjectMap(object_path);
    DCHECK(object_map);
    object_map->UpdateObjects(received_tags);
  }

  // nfc_client_helpers::DBusObjectMap::Delegate override.
  virtual NfcPropertySet* CreateProperties(
      dbus::ObjectProxy* object_proxy) OVERRIDE {
    Properties* properties = new Properties(
        object_proxy,
        base::Bind(&NfcTagClientImpl::OnPropertyChanged,
                   weak_ptr_factory_.GetWeakPtr(),
                   object_proxy->object_path()));
    properties->SetAllPropertiesReceivedCallback(
        base::Bind(&NfcTagClientImpl::OnPropertiesReceived,
                   weak_ptr_factory_.GetWeakPtr(),
                   object_proxy->object_path()));
    return properties;
  }

  // nfc_client_helpers::DBusObjectMap::Delegate override.
  virtual void ObjectAdded(const dbus::ObjectPath& object_path) OVERRIDE {
    FOR_EACH_OBSERVER(NfcTagClient::Observer, observers_,
                      TagAdded(object_path));
  }

  virtual void ObjectRemoved(const dbus::ObjectPath& object_path) OVERRIDE {
    FOR_EACH_OBSERVER(NfcTagClient::Observer, observers_,
                      TagRemoved(object_path));
  }

  // Called by NfcPropertySet when a property value is changed, either by
  // result of a signal or response to a GetAll() or Get() call.
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name) {
    VLOG(1) << "Tag property changed; Path: " << object_path.value()
            << " Property: " << property_name;
    FOR_EACH_OBSERVER(NfcTagClient::Observer, observers_,
                      TagPropertyChanged(object_path, property_name));
  }

  // Called by NfcPropertySet when all properties have been processed as a
  // result of a call to GetAll.
  void OnPropertiesReceived(const dbus::ObjectPath& object_path) {
    VLOG(1) << "All tag properties received; Path: " << object_path.value();
    FOR_EACH_OBSERVER(NfcTagClient::Observer, observers_,
                      TagPropertiesReceived(object_path));
  }

  // We maintain a pointer to the bus to be able to request proxies for
  // new NFC tags that appear.
  dbus::Bus* bus_;

  // List of observers interested in event notifications.
  ObserverList<NfcTagClient::Observer> observers_;

  // Mapping from object paths to object proxies and properties structures that
  // were already created by us. This stucture stores a different DBusObjectMap
  // for each known NFC adapter object path.
  ObjectProxyTree adapters_to_object_maps_;

  // The adapter client that we listen to events notifications from.
  NfcAdapterClient* adapter_client_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<NfcTagClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NfcTagClientImpl);
};

NfcTagClient::NfcTagClient() {
}

NfcTagClient::~NfcTagClient() {
}

NfcTagClient* NfcTagClient::Create(NfcAdapterClient* adapter_client) {
  return new NfcTagClientImpl(adapter_client);
}

}  // namespace chromeos
