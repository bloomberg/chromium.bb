// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_bluetooth_adapter_client.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_bluetooth_device_client.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// Default interval for delayed tasks.
const int kSimulationIntervalMs = 750;

}  // namespace

const char FakeBluetoothAdapterClient::kAdapterPath[] =
    "/fake/hci0";
const char FakeBluetoothAdapterClient::kAdapterName[] =
    "Fake Adapter";
const char FakeBluetoothAdapterClient::kAdapterAddress[] =
    "01:1A:2B:1A:2B:03";

const char FakeBluetoothAdapterClient::kSecondAdapterPath[] =
    "/fake/hci1";
const char FakeBluetoothAdapterClient::kSecondAdapterName[] =
    "Second Fake Adapter";
const char FakeBluetoothAdapterClient::kSecondAdapterAddress[] =
    "00:DE:51:10:01:00";

FakeBluetoothAdapterClient::Properties::Properties(
    const PropertyChangedCallback& callback)
    : BluetoothAdapterClient::Properties(
        NULL,
        bluetooth_adapter::kBluetoothAdapterInterface,
        callback) {
}

FakeBluetoothAdapterClient::Properties::~Properties() {
}

void FakeBluetoothAdapterClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  VLOG(1) << "Get " << property->name();
  callback.Run(false);
}

void FakeBluetoothAdapterClient::Properties::GetAll() {
  VLOG(1) << "GetAll";
}

void FakeBluetoothAdapterClient::Properties::Set(
    dbus::PropertyBase *property,
    dbus::PropertySet::SetCallback callback) {
  VLOG(1) << "Set " << property->name();
  if (property->name() == powered.name() ||
      property->name() == alias.name() ||
      property->name() == discoverable.name() ||
      property->name() == discoverable_timeout.name()) {
    callback.Run(true);
    property->ReplaceValueWithSetValue();
  } else {
    callback.Run(false);
  }
}

FakeBluetoothAdapterClient::FakeBluetoothAdapterClient()
    : visible_(true),
      second_visible_(false),
      discovering_count_(0),
      simulation_interval_ms_(kSimulationIntervalMs) {
  properties_.reset(new Properties(base::Bind(
      &FakeBluetoothAdapterClient::OnPropertyChanged, base::Unretained(this))));

  properties_->address.ReplaceValue(kAdapterAddress);
  properties_->name.ReplaceValue("Fake Adapter (Name)");
  properties_->alias.ReplaceValue(kAdapterName);
  properties_->pairable.ReplaceValue(true);

  second_properties_.reset(new Properties(base::Bind(
      &FakeBluetoothAdapterClient::OnPropertyChanged, base::Unretained(this))));

  second_properties_->address.ReplaceValue(kSecondAdapterAddress);
  second_properties_->name.ReplaceValue("Second Fake Adapter (Name)");
  second_properties_->alias.ReplaceValue(kSecondAdapterName);
  second_properties_->pairable.ReplaceValue(true);
}

FakeBluetoothAdapterClient::~FakeBluetoothAdapterClient() {
}

void FakeBluetoothAdapterClient::Init(dbus::Bus* bus) {
}

void FakeBluetoothAdapterClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeBluetoothAdapterClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::vector<dbus::ObjectPath> FakeBluetoothAdapterClient::GetAdapters() {
  std::vector<dbus::ObjectPath> object_paths;
  if (visible_)
    object_paths.push_back(dbus::ObjectPath(kAdapterPath));
  if (second_visible_)
    object_paths.push_back(dbus::ObjectPath(kSecondAdapterPath));
  return object_paths;
}

FakeBluetoothAdapterClient::Properties*
FakeBluetoothAdapterClient::GetProperties(const dbus::ObjectPath& object_path) {
  if (object_path == dbus::ObjectPath(kAdapterPath))
    return properties_.get();
  else if (object_path == dbus::ObjectPath(kSecondAdapterPath))
    return second_properties_.get();
  else
    return NULL;
}

void FakeBluetoothAdapterClient::StartDiscovery(
    const dbus::ObjectPath& object_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  if (object_path != dbus::ObjectPath(kAdapterPath)) {
    PostDelayedTask(base::Bind(error_callback, kNoResponseError, ""));
    return;
  }

  ++discovering_count_;
  VLOG(1) << "StartDiscovery: " << object_path.value() << ", "
          << "count is now " << discovering_count_;
  PostDelayedTask(callback);

  if (discovering_count_ == 1) {
    properties_->discovering.ReplaceValue(true);

    FakeBluetoothDeviceClient* device_client =
        static_cast<FakeBluetoothDeviceClient*>(
            DBusThreadManager::Get()->GetBluetoothDeviceClient());
    device_client->BeginDiscoverySimulation(dbus::ObjectPath(kAdapterPath));
  }
}

void FakeBluetoothAdapterClient::StopDiscovery(
    const dbus::ObjectPath& object_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  if (object_path != dbus::ObjectPath(kAdapterPath)) {
    PostDelayedTask(base::Bind(error_callback, kNoResponseError, ""));
    return;
  }

  if (!discovering_count_) {
    LOG(WARNING) << "StopDiscovery called when not discovering";
    PostDelayedTask(base::Bind(error_callback, kNoResponseError, ""));
    return;
  }

  --discovering_count_;
  VLOG(1) << "StopDiscovery: " << object_path.value() << ", "
          << "count is now " << discovering_count_;
  PostDelayedTask(callback);

  if (discovering_count_ == 0) {
    FakeBluetoothDeviceClient* device_client =
        static_cast<FakeBluetoothDeviceClient*>(
            DBusThreadManager::Get()->GetBluetoothDeviceClient());
    device_client->EndDiscoverySimulation(dbus::ObjectPath(kAdapterPath));

    if (simulation_interval_ms_ > 100) {
      device_client->BeginIncomingPairingSimulation(
          dbus::ObjectPath(kAdapterPath));
    }

    properties_->discovering.ReplaceValue(false);
  }
}

void FakeBluetoothAdapterClient::RemoveDevice(
    const dbus::ObjectPath& object_path,
    const dbus::ObjectPath& device_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  if (object_path != dbus::ObjectPath(kAdapterPath)) {
    error_callback.Run(kNoResponseError, "");
    return;
  }

  VLOG(1) << "RemoveDevice: " << object_path.value()
          << " " << device_path.value();
  callback.Run();

  FakeBluetoothDeviceClient* device_client =
      static_cast<FakeBluetoothDeviceClient*>(
          DBusThreadManager::Get()->GetBluetoothDeviceClient());
  device_client->RemoveDevice(dbus::ObjectPath(kAdapterPath), device_path);
}

void FakeBluetoothAdapterClient::SetSimulationIntervalMs(int interval_ms) {
  simulation_interval_ms_ = interval_ms;
}

void FakeBluetoothAdapterClient::SetVisible(
    bool visible) {
  if (visible && !visible_) {
    // Adapter becoming visible
    visible_ = visible;

    FOR_EACH_OBSERVER(BluetoothAdapterClient::Observer, observers_,
                      AdapterAdded(dbus::ObjectPath(kAdapterPath)));

  } else if (visible_ && !visible) {
    // Adapter becoming invisible
    visible_ = visible;

    FOR_EACH_OBSERVER(BluetoothAdapterClient::Observer, observers_,
                      AdapterRemoved(dbus::ObjectPath(kAdapterPath)));
  }
}

void FakeBluetoothAdapterClient::SetSecondVisible(
    bool visible) {
  if (visible && !second_visible_) {
    // Second adapter becoming visible
    second_visible_ = visible;

    FOR_EACH_OBSERVER(BluetoothAdapterClient::Observer, observers_,
                      AdapterAdded(dbus::ObjectPath(kSecondAdapterPath)));

  } else if (second_visible_ && !visible) {
    // Second adapter becoming invisible
    second_visible_ = visible;

    FOR_EACH_OBSERVER(BluetoothAdapterClient::Observer, observers_,
                      AdapterRemoved(dbus::ObjectPath(kSecondAdapterPath)));
  }
}

void FakeBluetoothAdapterClient::OnPropertyChanged(
    const std::string& property_name) {
  if (property_name == properties_->powered.name() &&
      !properties_->powered.value()) {
    VLOG(1) << "Adapter powered off";

    if (discovering_count_) {
      discovering_count_ = 0;
      properties_->discovering.ReplaceValue(false);
    }
  }

  FOR_EACH_OBSERVER(BluetoothAdapterClient::Observer, observers_,
                    AdapterPropertyChanged(dbus::ObjectPath(kAdapterPath),
                                           property_name));
}

void FakeBluetoothAdapterClient::PostDelayedTask(
    const base::Closure& callback) {
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, callback,
      base::TimeDelta::FromMilliseconds(simulation_interval_ms_));
}

}  // namespace chromeos
