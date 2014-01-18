// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/nfc_record_client.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/dbus/nfc_device_client.h"
#include "chromeos/dbus/nfc_tag_client.h"
#include "dbus/bus.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using chromeos::nfc_client_helpers::DBusObjectMap;
using chromeos::nfc_client_helpers::ObjectProxyTree;

namespace chromeos {

NfcRecordClient::Properties::Properties(
    dbus::ObjectProxy* object_proxy,
    const PropertyChangedCallback& callback)
    : NfcPropertySet(object_proxy,
                     nfc_record::kNfcRecordInterface,
                     callback) {
  RegisterProperty(nfc_record::kTypeProperty, &type);
  RegisterProperty(nfc_record::kLanguageProperty, &language);
  RegisterProperty(nfc_record::kEncodingProperty, &encoding);
  RegisterProperty(nfc_record::kRepresentationProperty, &representation);
  RegisterProperty(nfc_record::kUriProperty, &uri);
  RegisterProperty(nfc_record::kMimeTypeProperty, &mime_type);
  RegisterProperty(nfc_record::kSizeProperty, &size);
  RegisterProperty(nfc_record::kActionProperty, &action);
}

NfcRecordClient::Properties::~Properties() {
}

// The NfcRecordClient implementation used in production.
class NfcRecordClientImpl : public NfcRecordClient,
                            public NfcDeviceClient::Observer,
                            public NfcTagClient::Observer,
                            public DBusObjectMap::Delegate {
 public:
  explicit NfcRecordClientImpl(NfcDeviceClient* device_client,
                               NfcTagClient* tag_client)
      : bus_(NULL),
        device_client_(device_client),
        tag_client_(tag_client),
        weak_ptr_factory_(this) {
    DCHECK(device_client);
    DCHECK(tag_client);
  }

  virtual ~NfcRecordClientImpl() {
    DCHECK(device_client_);
    DCHECK(tag_client_);
    device_client_->RemoveObserver(this);
    tag_client_->RemoveObserver(this);
  }

  // NfcRecordClient override.
  virtual void AddObserver(NfcRecordClient::Observer* observer) OVERRIDE {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  // NfcRecordClient override.
  virtual void RemoveObserver(NfcRecordClient::Observer* observer) OVERRIDE {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  virtual std::vector<dbus::ObjectPath> GetRecordsForDevice(
      const dbus::ObjectPath& device_path) OVERRIDE {
    DBusObjectMap* object_map =
        devices_and_tags_to_object_maps_.GetObjectMap(device_path);
    if (!object_map)
      return std::vector<dbus::ObjectPath>();
    return object_map->GetObjectPaths();
  }

  virtual std::vector<dbus::ObjectPath> GetRecordsForTag(
      const dbus::ObjectPath& tag_path) OVERRIDE {
    return GetRecordsForDevice(tag_path);
  }

  // NfcRecordClient override.
  virtual Properties* GetProperties(
      const dbus::ObjectPath& object_path) OVERRIDE {
    return static_cast<Properties*>(
        devices_and_tags_to_object_maps_.FindObjectProperties(object_path));
  }

 protected:
  // DBusClient override.
  virtual void Init(dbus::Bus* bus) OVERRIDE {
    VLOG(1) << "Creating NfcRecordClient impl";
    DCHECK(bus);
    bus_ = bus;
    DCHECK(device_client_);
    DCHECK(tag_client_);
    device_client_->AddObserver(this);
    tag_client_->AddObserver(this);
  }

 private:
  // NfcDeviceClient::Observer override.
  virtual void DeviceAdded(const dbus::ObjectPath& object_path) OVERRIDE {
    VLOG(1) << "Device added. Creating map for record proxies belonging to "
            << "device: " << object_path.value();
    devices_and_tags_to_object_maps_.CreateObjectMap(
        object_path, nfc_record::kNfcRecordServiceName, this, bus_);
  }

  // NfcDeviceClient::Observer override.
  virtual void DeviceRemoved(const dbus::ObjectPath& object_path) OVERRIDE {
    // Neard doesn't send out property changed signals for the records that
    // are removed when the device they belong to is removed. Clean up the
    // object proxies for records that belong to the removed device.
    // Note: DBusObjectMap guarantees that the Properties structure for the
    // removed adapter will be valid before this method returns.
    VLOG(1) << "Device removed. Cleaning up record proxies belonging to "
            << "device: " << object_path.value();
    devices_and_tags_to_object_maps_.RemoveObjectMap(object_path);
  }

  // NfcDeviceClient::Observer override.
  virtual void DevicePropertyChanged(
      const dbus::ObjectPath& object_path,
      const std::string& property_name) OVERRIDE {
    // Update the record proxies using records from the device.
    DCHECK(device_client_);
    NfcDeviceClient::Properties* device_properties =
        device_client_->GetProperties(object_path);

    // Ignore changes to properties other than "Records".
    if (property_name != device_properties->records.name())
      return;

    // Update known records.
    VLOG(1) << "NFC records changed.";
    const std::vector<dbus::ObjectPath>& received_records =
        device_properties->records.value();
    DBusObjectMap* object_map =
        devices_and_tags_to_object_maps_.GetObjectMap(object_path);
    DCHECK(object_map);
    object_map->UpdateObjects(received_records);
  }

  // NfcTagClient::Observer override.
  virtual void TagAdded(const dbus::ObjectPath& object_path) OVERRIDE {
    VLOG(1) << "Tag added. Creating map for record proxies belonging to "
            << "tag: " << object_path.value();
    devices_and_tags_to_object_maps_.CreateObjectMap(
        object_path, nfc_record::kNfcRecordServiceName, this, bus_);
  }

  // NfcTagClient::Observer override.
  virtual void TagRemoved(const dbus::ObjectPath& object_path) OVERRIDE {
    // Neard doesn't send out property changed signals for the records that
    // are removed when the tag they belong to is removed. Clean up the
    // object proxies for records that belong to the removed tag.
    // Note: DBusObjectMap guarantees that the Properties structure for the
    // removed adapter will be valid before this method returns.
    VLOG(1) << "Tag removed. Cleaning up record proxies belonging to "
            << "tag: " << object_path.value();
    devices_and_tags_to_object_maps_.RemoveObjectMap(object_path);
  }

  // NfcTagClient::Observer override.
  virtual void TagPropertyChanged(const dbus::ObjectPath& object_path,
                                  const std::string& property_name) OVERRIDE {
    // Update the record proxies using records from the tag.
    DCHECK(device_client_);
    NfcTagClient::Properties* tag_properties =
        tag_client_->GetProperties(object_path);

    // Ignore changes to properties other than "Records".
    if (property_name != tag_properties->records.name())
      return;

    // Update known records.
    VLOG(1) << "NFC records changed.";
    const std::vector<dbus::ObjectPath>& received_records =
        tag_properties->records.value();
    DBusObjectMap* object_map =
        devices_and_tags_to_object_maps_.GetObjectMap(object_path);
    DCHECK(object_map);
    object_map->UpdateObjects(received_records);

    // When rewriting the record to a tag, neard fires a property changed
    // signal for the tags "Records" property, without creating a new object
    // path. Sync the properties of all records here, in case Update objects
    // doesn't do it.
    VLOG(1) << "Fetch properties for all records.";
    object_map->RefreshAllProperties();
  }

  // nfc_client_helpers::DBusObjectMap::Delegate override.
  virtual NfcPropertySet* CreateProperties(
      dbus::ObjectProxy* object_proxy) OVERRIDE {
    Properties* properties = new Properties(
        object_proxy,
        base::Bind(&NfcRecordClientImpl::OnPropertyChanged,
                   weak_ptr_factory_.GetWeakPtr(),
                   object_proxy->object_path()));
    properties->SetAllPropertiesReceivedCallback(
        base::Bind(&NfcRecordClientImpl::OnPropertiesReceived,
                   weak_ptr_factory_.GetWeakPtr(),
                   object_proxy->object_path()));
    return properties;
  }

  // nfc_client_helpers::DBusObjectMap::Delegate override.
  virtual void ObjectAdded(const dbus::ObjectPath& object_path) OVERRIDE {
    FOR_EACH_OBSERVER(NfcRecordClient::Observer, observers_,
                      RecordAdded(object_path));
  }

  // nfc_client_helpers::DBusObjectMap::Delegate override.
  virtual void ObjectRemoved(const dbus::ObjectPath& object_path) OVERRIDE {
    FOR_EACH_OBSERVER(NfcRecordClient::Observer, observers_,
                      RecordRemoved(object_path));
  }

  // Called by NfcPropertySet when a property value is changed, either by
  // result of a signal or response to a GetAll() or Get() call.
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name) {
    VLOG(1) << "Record property changed; Path: " << object_path.value()
            << " Property: " << property_name;
    FOR_EACH_OBSERVER(NfcRecordClient::Observer, observers_,
                      RecordPropertyChanged(object_path, property_name));
  }

  // Called by NfcPropertySet when all properties have been processed as a
  // result of a call to GetAll.
  void OnPropertiesReceived(const dbus::ObjectPath& object_path) {
    VLOG(1) << "All record properties received; Path: " << object_path.value();
    FOR_EACH_OBSERVER(NfcRecordClient::Observer, observers_,
                      RecordPropertiesReceived(object_path));
  }

  // We maintain a pointer to the bus to be able to request proxies for
  // new NFC records that appear.
  dbus::Bus* bus_;

  // List of observers interested in event notifications.
  ObserverList<NfcRecordClient::Observer> observers_;

  // Mapping from object paths to object proxies and properties structures that
  // were already created by us. Record objects belong to either Tag or Device
  // objects. This structure stores a different DBusObjectMap for each known
  // device and tag.
  ObjectProxyTree devices_and_tags_to_object_maps_;

  // The device and tag clients that we listen to events notifications from.
  NfcDeviceClient* device_client_;
  NfcTagClient* tag_client_;
  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<NfcRecordClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NfcRecordClientImpl);
};

NfcRecordClient::NfcRecordClient() {
}

NfcRecordClient::~NfcRecordClient() {
}

NfcRecordClient* NfcRecordClient::Create(NfcDeviceClient* device_client,
                                         NfcTagClient* tag_client) {
  return new NfcRecordClientImpl(device_client, tag_client);
}

}  // namespace chromeos
