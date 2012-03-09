// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/bluetooth/bluetooth_adapter.h"

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_device.h"
#include "chrome/browser/chromeos/dbus/bluetooth_adapter_client.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"

namespace {

// Used to ignore whether or not our method calls work, since we were
// ignoring them before I added callbacks to the methods.
void EmptyAdapterCallback(const dbus::ObjectPath& object_path,
                          bool success) {
}

};

namespace chromeos {

class BluetoothAdapterImpl : public BluetoothAdapter,
                             public BluetoothAdapterClient::Observer {
 public:
  explicit BluetoothAdapterImpl(const std::string& id) : id_(id) {
    DBusThreadManager* dbus_thread_manager = DBusThreadManager::Get();
    DCHECK(dbus_thread_manager);
    bluetooth_adapter_client_ =
        dbus_thread_manager->GetBluetoothAdapterClient();
    DCHECK(bluetooth_adapter_client_);
    bluetooth_adapter_client_->AddObserver(this);
  }

  virtual ~BluetoothAdapterImpl() {
    DCHECK(bluetooth_adapter_client_);
    bluetooth_adapter_client_->RemoveObserver(this);
  }

  // BluetoothAdapter override.
  virtual void AddObserver(BluetoothAdapter::Observer* observer) OVERRIDE {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  // BluetoothAdapter override.
  virtual void RemoveObserver(BluetoothAdapter::Observer* observer) OVERRIDE {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  // BluetoothAdapter override.
  virtual const std::string& Id() const OVERRIDE {
    return id_;
  }

  // BluetoothAdapter override.
  virtual void StartDiscovery() OVERRIDE {
    DCHECK(bluetooth_adapter_client_);
    bluetooth_adapter_client_->StartDiscovery(
        dbus::ObjectPath(id_),
        base::Bind(&EmptyAdapterCallback));
  }

  // BluetoothAdapter override.
  virtual void StopDiscovery() OVERRIDE {
    DCHECK(bluetooth_adapter_client_);
    bluetooth_adapter_client_->StopDiscovery(dbus::ObjectPath(id_),
                                             base::Bind(&EmptyAdapterCallback));
  }

  // BluetoothAdapterClient::Observer override.
  virtual void AdapterPropertyChanged(const dbus::ObjectPath& object_path,
                                      const std::string& property_name)
      OVERRIDE {
    if (object_path.value() != id_)
      return;

    DCHECK(bluetooth_adapter_client_);
    BluetoothAdapterClient::Properties* properties =
        bluetooth_adapter_client_->GetProperties(object_path);

    DCHECK(properties);
    if (property_name != properties->discovering.name())
      return;

    bool discovering = properties->discovering.value();

    DVLOG(1) << id_ << ": object_path = " << object_path.value()
            << ", Discovering = " << discovering;
    if (discovering) {
      FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                        DiscoveryStarted(object_path.value()));
    } else {
      FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                        DiscoveryEnded(object_path.value()));
    }
  }

  // BluetoothAdapterClient::Observer override.
  virtual void DeviceFound(const dbus::ObjectPath& object_path,
                           const std::string& address,
                           const BluetoothDeviceClient::Properties& properties)
      OVERRIDE {
    if (object_path.value() != id_) {
      return;
    }
    // TODO(vlaviano): later, we will want to persist the device.
    scoped_ptr<BluetoothDevice> device(
        BluetoothDevice::Create(properties.address.value(),
                                properties.bluetooth_class.value(),
                                properties.icon.value(),
                                properties.name.value(),
                                properties.paired.value(),
                                properties.connected.value()));
    if (device.get() != NULL) {
      FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                        DeviceFound(object_path.value(), device.get()));
    } else {
      LOG(WARNING) << "Could not create BluetoothDevice from properties.";
    }
  }

  // BluetoothAdapterClient::Observer override.
  virtual void DeviceDisappeared(const dbus::ObjectPath& object_path,
                                 const std::string& address) OVERRIDE {
    if (object_path.value() != id_) {
      return;
    }
    // For now, we don't propagate this event to our observers.
  }

 private:
  // Owned by the dbus thread manager.
  BluetoothAdapterClient* bluetooth_adapter_client_;

  ObserverList<BluetoothAdapter::Observer> observers_;

  // An opaque identifier that we provide to clients.
  // We use the dbus object path for the adapter as the id.
  const std::string id_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterImpl);
};

BluetoothAdapter::BluetoothAdapter() {
}

BluetoothAdapter::~BluetoothAdapter() {
}

// static
BluetoothAdapter* BluetoothAdapter::Create(const std::string& id) {
  return new BluetoothAdapterImpl(id);
}

}  // namespace chromeos
