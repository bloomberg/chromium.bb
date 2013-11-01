// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/nfc_device_client.h"

#include <set>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/stringprintf.h"
#include "chromeos/dbus/fake_nfc_device_client.h"
#include "chromeos/dbus/nfc_adapter_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using chromeos::nfc_client_helpers::DBusObjectMap;

namespace chromeos {

NfcDeviceClient::Properties::Properties(
    dbus::ObjectProxy* object_proxy,
    const PropertyChangedCallback& callback)
    : NfcPropertySet(object_proxy,
                     nfc_device::kNfcDeviceInterface,
                     callback) {
  RegisterProperty(nfc_device::kRecordsProperty, &records);
}

NfcDeviceClient::Properties::~Properties() {
}

// The NfcDeviceClient implementation used in production.
class NfcDeviceClientImpl : public NfcDeviceClient,
                            public NfcAdapterClient::Observer,
                            public DBusObjectMap::Delegate {
 public:
  explicit NfcDeviceClientImpl(NfcAdapterClient* adapter_client)
      : bus_(NULL),
        adapter_client_(adapter_client),
        weak_ptr_factory_(this) {
    DCHECK(adapter_client);
  }

  virtual ~NfcDeviceClientImpl() {
    adapter_client_->RemoveObserver(this);
  }

  // NfcDeviceClient override.
  virtual void AddObserver(NfcDeviceClient::Observer* observer) OVERRIDE {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  // NfcDeviceClient override.
  virtual void RemoveObserver(NfcDeviceClient::Observer* observer) OVERRIDE {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  // NfcDeviceClient override.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path)
      OVERRIDE {
    return static_cast<Properties*>(
        object_map_->GetObjectProperties(object_path));
  }

  // NfcDeviceClient override.
  virtual void Push(
      const dbus::ObjectPath& object_path,
      const RecordAttributes& attributes,
      const base::Closure& callback,
      const nfc_client_helpers::ErrorCallback& error_callback) OVERRIDE {
    dbus::ObjectProxy* object_proxy = object_map_->GetObjectProxy(object_path);
    if (!object_proxy) {
      std::string error_message =
          base::StringPrintf(
              "NFC device with object path \"%s\" does not exist.",
              object_path.value().c_str());
      LOG(ERROR) << error_message;
      error_callback.Run(nfc_client_helpers::kUnknownObjectError,
                         error_message);
      return;
    }

    // |attributes| should not be empty.
    if (attributes.empty()) {
      std::string error_message =
          "Cannot push data to device with empty arguments.";
      LOG(ERROR) << error_message;
      error_callback.Run(nfc_error::kInvalidArguments, error_message);
      return;
    }

    // Create the arguments.
    dbus::MethodCall method_call(nfc_device::kNfcDeviceInterface,
                                 nfc_device::kPush);
    dbus::MessageWriter writer(&method_call);
    dbus::MessageWriter array_writer(NULL);
    dbus::MessageWriter dict_entry_writer(NULL);
    writer.OpenArray("{sv}", &array_writer);
    for (RecordAttributes::const_iterator iter = attributes.begin();
         iter != attributes.end(); ++iter) {
      array_writer.OpenDictEntry(&dict_entry_writer);
      dict_entry_writer.AppendString(iter->first);
      dict_entry_writer.AppendVariantOfString(iter->second);
      array_writer.CloseContainer(&dict_entry_writer);
    }
    writer.CloseContainer(&array_writer);

    object_proxy->CallMethodWithErrorCallback(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&nfc_client_helpers::OnSuccess, callback),
        base::Bind(&nfc_client_helpers::OnError, error_callback));
  }

 protected:
  // DBusClient override.
  virtual void Init(dbus::Bus* bus) OVERRIDE {
    VLOG(1) << "Creating NfcDeviceClientImpl";
    DCHECK(bus);
    bus_ = bus;
    object_map_.reset(new DBusObjectMap(
        nfc_device::kNfcDeviceServiceName, this, bus));
    DCHECK(adapter_client_);
    adapter_client_->AddObserver(this);
  }

  // NfcAdapterClient::Observer override.
  virtual void AdapterPropertyChanged(
      const dbus::ObjectPath& object_path,
      const std::string& property_name) OVERRIDE {
    DCHECK(adapter_client_);
    NfcAdapterClient::Properties *adapter_properties =
        adapter_client_->GetProperties(object_path);

    // Ignore changes to properties other than "Devices".
    if (property_name != adapter_properties->devices.name())
      return;

    VLOG(1) << "NFC devices changed.";

    // Update the known devices.
    const std::vector<dbus::ObjectPath>& received_devices =
        adapter_properties->devices.value();
    object_map_->UpdateObjects(received_devices);
  }

  // nfc_client_helpers::DBusObjectMap::Delegate override.
  virtual NfcPropertySet* CreateProperties(
      dbus::ObjectProxy* object_proxy) OVERRIDE {
    return new Properties(
        object_proxy,
        base::Bind(&NfcDeviceClientImpl::OnPropertyChanged,
                   weak_ptr_factory_.GetWeakPtr(),
                   object_proxy->object_path()));
  }

  // nfc_client_helpers::DBusObjectMap::Delegate override.
  virtual void ObjectAdded(const dbus::ObjectPath& object_path) OVERRIDE {
    FOR_EACH_OBSERVER(NfcDeviceClient::Observer, observers_,
                      DeviceFound(object_path));
  }

  virtual void ObjectRemoved(const dbus::ObjectPath& object_path) OVERRIDE {
    FOR_EACH_OBSERVER(NfcDeviceClient::Observer, observers_,
                      DeviceLost(object_path));
  }

  // Called by NfcPropertySet when a property value is changed, either by
  // result of a signal or response to a GetAll() or Get() call.
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name) {
    VLOG(1) << "Device property changed; Path: " << object_path.value()
            << " Property: " << property_name;
    FOR_EACH_OBSERVER(NfcDeviceClient::Observer, observers_,
                      DevicePropertyChanged(object_path, property_name));
  }

 private:
  // We maintain a pointer to the bus to be able to request proxies for
  // new NFC devices that appear.
  dbus::Bus* bus_;

  // Mapping from object paths to object proxies and properties structures that
  // were already created by us.
  scoped_ptr<DBusObjectMap> object_map_;

  // The manager client that we listen to events notifications from.
  NfcAdapterClient* adapter_client_;

  // List of observers interested in event notifications.
  ObserverList<NfcDeviceClient::Observer> observers_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<NfcDeviceClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NfcDeviceClientImpl);
};

NfcDeviceClient::NfcDeviceClient() {
}

NfcDeviceClient::~NfcDeviceClient() {
}

NfcDeviceClient* NfcDeviceClient::Create(DBusClientImplementationType type,
                                         NfcAdapterClient* adapter_client) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new NfcDeviceClientImpl(adapter_client);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new FakeNfcDeviceClient();
}

}  // namespace chromeos
