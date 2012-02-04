// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/bluetooth/bluetooth_adapter.h"

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_device.h"
#include "chrome/browser/chromeos/dbus/bluetooth_adapter_client.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"

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
    bluetooth_adapter_client_->AddObserver(this, id_);
  }

  virtual ~BluetoothAdapterImpl() {
    DCHECK(bluetooth_adapter_client_);
    bluetooth_adapter_client_->RemoveObserver(this, id_);
  }

  virtual void AddObserver(BluetoothAdapter::Observer* observer) {
    VLOG(1) << id_ << ": AddObserver";
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  virtual void RemoveObserver(BluetoothAdapter::Observer* observer) {
    VLOG(1) << id_ << ": RemoveObserver";
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  virtual const std::string& Id() const {
    return id_;
  }

  virtual void StartDiscovery() {
    VLOG(1) << id_ << ": StartDiscovery";
    DCHECK(bluetooth_adapter_client_);
    bluetooth_adapter_client_->StartDiscovery(id_);
  }

  virtual void StopDiscovery() {
    VLOG(1) << id_ << ": StopDiscovery";
    DCHECK(bluetooth_adapter_client_);
    bluetooth_adapter_client_->StopDiscovery(id_);
  }

  // BluetoothAdapterClient::Observer override.
  virtual void DiscoveringPropertyChanged(const std::string& object_path,
                                          bool discovering) {
    VLOG(1) << id_ << ": object_path = " << object_path << ", Discovering = "
            << discovering;
    if (object_path != id_) {
      return;
    }
    if (discovering) {
      FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                        DiscoveryStarted(object_path));
    } else {
      FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                        DiscoveryEnded(object_path));
    }
  }

  // BluetoothAdapterClient::Observer override.
  virtual void DeviceFound(const std::string& object_path,
                           const std::string& address,
                           const base::DictionaryValue& device_properties) {
    VLOG(1) << id_ << ": object_path = " << object_path << ", Device found: "
            << address << " (with " << device_properties.size()
            << " properties)";
    if (object_path != id_) {
      return;
    }
    // TODO(vlaviano): later, we will want to persist the device.
    scoped_ptr<BluetoothDevice> device(
        BluetoothDevice::Create(device_properties));
    if (device.get() != NULL) {
      FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                        DeviceFound(object_path, device.get()));
    } else {
      LOG(WARNING) << "Could not create BluetoothDevice from properties.";
    }
  }

  // BluetoothAdapterClient::Observer override.
  virtual void DeviceDisappeared(const std::string& object_path,
                                 const std::string& address) {
    VLOG(1) << id_ << ": object_path = " << object_path
            << ", Device disappeared: " << address;
    if (object_path != id_) {
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
