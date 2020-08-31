// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"

#include <map>
#include <utility>

#include "base/bind_helpers.h"
#include "base/callback_forward.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "dbus/bus.h"
#include "device/bluetooth/bluez/bluetooth_service_record_bluez.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

namespace {

// Default interval for delayed tasks.
const int kSimulationIntervalMs = 750;

}  // namespace

const char FakeBluetoothAdapterClient::kAdapterPath[] = "/fake/hci0";
const char FakeBluetoothAdapterClient::kAdapterName[] = "Fake Adapter";
const char FakeBluetoothAdapterClient::kAdapterAddress[] = "01:1A:2B:1A:2B:03";

const char FakeBluetoothAdapterClient::kSecondAdapterPath[] = "/fake/hci1";
const char FakeBluetoothAdapterClient::kSecondAdapterName[] =
    "Second Fake Adapter";
const char FakeBluetoothAdapterClient::kSecondAdapterAddress[] =
    "00:DE:51:10:01:00";

FakeBluetoothAdapterClient::Properties::Properties(
    const PropertyChangedCallback& callback)
    : BluetoothAdapterClient::Properties(
          NULL,
          bluetooth_adapter::kBluetoothAdapterInterface,
          callback) {}

FakeBluetoothAdapterClient::Properties::~Properties() = default;

void FakeBluetoothAdapterClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  DVLOG(1) << "Get " << property->name();
  std::move(callback).Run(false);
}

void FakeBluetoothAdapterClient::Properties::GetAll() {
  DVLOG(1) << "GetAll";
}

void FakeBluetoothAdapterClient::Properties::Set(
    dbus::PropertyBase* property,
    dbus::PropertySet::SetCallback callback) {
  DVLOG(1) << "Set " << property->name();
  if (property->name() == powered.name() || property->name() == alias.name() ||
      property->name() == discoverable.name() ||
      property->name() == discoverable_timeout.name()) {
    std::move(callback).Run(true);
    property->ReplaceValueWithSetValue();
  } else {
    std::move(callback).Run(false);
  }
}

FakeBluetoothAdapterClient::FakeBluetoothAdapterClient()
    : visible_(true),
      second_visible_(false),
      discovering_count_(0),
      pause_count_(0),
      unpause_count_(0),
      set_discovery_filter_should_fail_(false),
      simulation_interval_ms_(kSimulationIntervalMs),
      last_handle_(0) {
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

FakeBluetoothAdapterClient::~FakeBluetoothAdapterClient() = default;

void FakeBluetoothAdapterClient::Init(
    dbus::Bus* bus,
    const std::string& bluetooth_service_name) {}

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
    ResponseCallback callback) {
  if (object_path != dbus::ObjectPath(kAdapterPath)) {
    PostDelayedTask(
        base::BindOnce(std::move(callback), Error(kNoResponseError, "")));
    return;
  }

  if (set_start_discovery_should_fail_) {
    set_start_discovery_should_fail_ = false;
    PostDelayedTask(
        base::BindOnce(std::move(callback), Error(kUnknownAdapterError, "")));
    return;
  }

  ++discovering_count_;
  DVLOG(1) << "StartDiscovery: " << object_path.value() << ", "
           << "count is now " << discovering_count_;
  PostDelayedTask(base::BindOnce(std::move(callback), base::nullopt));

  if (discovering_count_ == 1) {
    PostDelayedTask(
        base::BindOnce(&FakeBluetoothAdapterClient::UpdateDiscoveringProperty,
                       weak_ptr_factory_.GetWeakPtr(), /*discovering=*/true));
    FakeBluetoothDeviceClient* device_client =
        static_cast<FakeBluetoothDeviceClient*>(
            bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient());
    device_client->BeginDiscoverySimulation(dbus::ObjectPath(kAdapterPath));
  }
}

void FakeBluetoothAdapterClient::StopDiscovery(
    const dbus::ObjectPath& object_path,
    ResponseCallback callback) {
  if (object_path != dbus::ObjectPath(kAdapterPath)) {
    PostDelayedTask(
        base::BindOnce(std::move(callback), Error(kNoResponseError, "")));
    return;
  }

  if (!discovering_count_) {
    LOG(WARNING) << "StopDiscovery called when not discovering";
    PostDelayedTask(
        base::BindOnce(std::move(callback), Error(kNoResponseError, "")));
    return;
  }

  --discovering_count_;
  DVLOG(1) << "StopDiscovery: " << object_path.value() << ", "
           << "count is now " << discovering_count_;
  PostDelayedTask(base::BindOnce(std::move(callback), base::nullopt));

  if (discovering_count_ == 0) {
    FakeBluetoothDeviceClient* device_client =
        static_cast<FakeBluetoothDeviceClient*>(
            bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient());
    device_client->EndDiscoverySimulation(dbus::ObjectPath(kAdapterPath));

    if (simulation_interval_ms_ > 100) {
      device_client->BeginIncomingPairingSimulation(
          dbus::ObjectPath(kAdapterPath));
    }

    discovery_filter_.reset();
    PostDelayedTask(
        base::BindOnce(&FakeBluetoothAdapterClient::UpdateDiscoveringProperty,
                       weak_ptr_factory_.GetWeakPtr(), /*discovering=*/false));
  }
}

void FakeBluetoothAdapterClient::UpdateDiscoveringProperty(bool discovering) {
  properties_->discovering.ReplaceValue(discovering);
}

void FakeBluetoothAdapterClient::PauseDiscovery(
    const dbus::ObjectPath& object_path,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  ++pause_count_;
  std::move(callback).Run();
}

void FakeBluetoothAdapterClient::UnpauseDiscovery(
    const dbus::ObjectPath& object_path,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  ++unpause_count_;
  std::move(callback).Run();
}

void FakeBluetoothAdapterClient::RemoveDevice(
    const dbus::ObjectPath& object_path,
    const dbus::ObjectPath& device_path,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  if (object_path != dbus::ObjectPath(kAdapterPath)) {
    std::move(error_callback).Run(kNoResponseError, "");
    return;
  }

  DVLOG(1) << "RemoveDevice: " << object_path.value() << " "
           << device_path.value();
  std::move(callback).Run();

  FakeBluetoothDeviceClient* device_client =
      static_cast<FakeBluetoothDeviceClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient());
  device_client->RemoveDevice(dbus::ObjectPath(kAdapterPath), device_path);
}

void FakeBluetoothAdapterClient::MakeSetDiscoveryFilterFail() {
  set_discovery_filter_should_fail_ = true;
}

void FakeBluetoothAdapterClient::MakeStartDiscoveryFail() {
  set_start_discovery_should_fail_ = true;
}

void FakeBluetoothAdapterClient::SetDiscoveryFilter(
    const dbus::ObjectPath& object_path,
    const DiscoveryFilter& discovery_filter,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  if (object_path != dbus::ObjectPath(kAdapterPath)) {
    PostDelayedTask(
        base::BindOnce(std::move(error_callback), kNoResponseError, ""));
    return;
  }
  DVLOG(1) << "SetDiscoveryFilter: " << object_path.value();

  if (set_discovery_filter_should_fail_) {
    PostDelayedTask(
        base::BindOnce(std::move(error_callback), kNoResponseError, ""));
    set_discovery_filter_should_fail_ = false;
    return;
  }

  discovery_filter_.reset(new DiscoveryFilter());
  discovery_filter_->CopyFrom(discovery_filter);
  PostDelayedTask(std::move(callback));
}

void FakeBluetoothAdapterClient::CreateServiceRecord(
    const dbus::ObjectPath& object_path,
    const bluez::BluetoothServiceRecordBlueZ& record,
    ServiceRecordCallback callback,
    ErrorCallback error_callback) {
  ++last_handle_;
  records_.insert(
      std::pair<uint32_t, BluetoothServiceRecordBlueZ>(last_handle_, record));
  std::move(callback).Run(last_handle_);
}

void FakeBluetoothAdapterClient::RemoveServiceRecord(
    const dbus::ObjectPath& object_path,
    uint32_t handle,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  auto it = records_.find(handle);
  if (it == records_.end()) {
    std::move(error_callback)
        .Run(bluetooth_adapter::kErrorDoesNotExist,
             "Service record does not exist.");
    return;
  }
  records_.erase(it);
  std::move(callback).Run();
}

void FakeBluetoothAdapterClient::SetSimulationIntervalMs(int interval_ms) {
  simulation_interval_ms_ = interval_ms;
}

BluetoothAdapterClient::DiscoveryFilter*
FakeBluetoothAdapterClient::GetDiscoveryFilter() {
  return discovery_filter_.get();
}

void FakeBluetoothAdapterClient::SetVisible(bool visible) {
  if (visible && !visible_) {
    // Adapter becoming visible
    visible_ = visible;

    for (auto& observer : observers_)
      observer.AdapterAdded(dbus::ObjectPath(kAdapterPath));

  } else if (visible_ && !visible) {
    // Adapter becoming invisible
    visible_ = visible;

    for (auto& observer : observers_)
      observer.AdapterRemoved(dbus::ObjectPath(kAdapterPath));
  }
}

void FakeBluetoothAdapterClient::SetSecondVisible(bool visible) {
  if (visible && !second_visible_) {
    // Second adapter becoming visible
    second_visible_ = visible;

    for (auto& observer : observers_)
      observer.AdapterAdded(dbus::ObjectPath(kSecondAdapterPath));

  } else if (second_visible_ && !visible) {
    // Second adapter becoming invisible
    second_visible_ = visible;

    for (auto& observer : observers_)
      observer.AdapterRemoved(dbus::ObjectPath(kSecondAdapterPath));
  }
}

void FakeBluetoothAdapterClient::SetUUIDs(
    const std::vector<std::string>& uuids) {
  properties_->uuids.ReplaceValue(uuids);
}

void FakeBluetoothAdapterClient::SetSecondUUIDs(
    const std::vector<std::string>& uuids) {
  second_properties_->uuids.ReplaceValue(uuids);
}

void FakeBluetoothAdapterClient::SetDiscoverableTimeout(uint32_t timeout) {
  properties_->discoverable_timeout.ReplaceValue(timeout);
}

void FakeBluetoothAdapterClient::OnPropertyChanged(
    const std::string& property_name) {
  if (property_name == properties_->powered.name() &&
      !properties_->powered.value()) {
    DVLOG(1) << "Adapter powered off";

    if (discovering_count_) {
      discovering_count_ = 0;
      properties_->discovering.ReplaceValue(false);
    }
  }

  for (auto& observer : observers_) {
    observer.AdapterPropertyChanged(dbus::ObjectPath(kAdapterPath),
                                    property_name);
  }
}

void FakeBluetoothAdapterClient::PostDelayedTask(base::OnceClosure callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, std::move(callback),
      base::TimeDelta::FromMilliseconds(simulation_interval_ms_));
}

}  // namespace bluez
