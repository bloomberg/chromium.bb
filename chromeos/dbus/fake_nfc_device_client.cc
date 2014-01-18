// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_nfc_device_client.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_nfc_adapter_client.h"
#include "chromeos/dbus/fake_nfc_record_client.h"
#include "chromeos/dbus/nfc_client_helpers.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

using nfc_client_helpers::ObjectPathVector;

const char FakeNfcDeviceClient::kDevicePath[] = "/fake/device0";
const int FakeNfcDeviceClient::kDefaultSimulationTimeoutMilliseconds = 10000;

FakeNfcDeviceClient::Properties::Properties(
    const PropertyChangedCallback& callback)
    : NfcDeviceClient::Properties(NULL, callback) {
}

FakeNfcDeviceClient::Properties::~Properties() {
}

void FakeNfcDeviceClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  VLOG(1) << "Get " << property->name();
  callback.Run(false);
}

void FakeNfcDeviceClient::Properties::GetAll() {
  VLOG(1) << "GetAll";
}

void FakeNfcDeviceClient::Properties::Set(
    dbus::PropertyBase* property,
    dbus::PropertySet::SetCallback callback) {
  VLOG(1) << "Set " << property->name();
  callback.Run(false);
}

FakeNfcDeviceClient::FakeNfcDeviceClient()
    : pairing_started_(false),
      device_visible_(false),
      simulation_timeout_(kDefaultSimulationTimeoutMilliseconds) {
  VLOG(1) << "Creating FakeNfcDeviceClient";

  properties_.reset(new Properties(
      base::Bind(&FakeNfcDeviceClient::OnPropertyChanged,
                 base::Unretained(this),
                 dbus::ObjectPath(kDevicePath))));
}

FakeNfcDeviceClient::~FakeNfcDeviceClient() {
}

void FakeNfcDeviceClient::Init(dbus::Bus* bus) {
}

void FakeNfcDeviceClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeNfcDeviceClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::vector<dbus::ObjectPath> FakeNfcDeviceClient::GetDevicesForAdapter(
      const dbus::ObjectPath& adapter_path) {
  std::vector<dbus::ObjectPath> device_paths;
  if (device_visible_ &&
      adapter_path.value() == FakeNfcAdapterClient::kAdapterPath0)
    device_paths.push_back(dbus::ObjectPath(kDevicePath));
  return device_paths;
}

FakeNfcDeviceClient::Properties*
FakeNfcDeviceClient::GetProperties(const dbus::ObjectPath& object_path) {
  if (!device_visible_)
    return NULL;
  return properties_.get();
}

void FakeNfcDeviceClient::Push(
    const dbus::ObjectPath& object_path,
    const base::DictionaryValue& attributes,
    const base::Closure& callback,
    const nfc_client_helpers::ErrorCallback& error_callback) {
  VLOG(1) << "FakeNfcDeviceClient::Write called.";

  // Success!
  if (!device_visible_) {
    LOG(ERROR) << "Device not visible. Cannot push record.";
    error_callback.Run(nfc_error::kDoesNotExist, "No such device.");
    return;
  }
  callback.Run();
}

void FakeNfcDeviceClient::BeginPairingSimulation(int visibility_delay,
                                                 int record_push_delay) {
  if (pairing_started_) {
    VLOG(1) << "Simulation already started.";
    return;
  }
  DCHECK(!device_visible_);
  DCHECK(visibility_delay >= 0);

  pairing_started_ = true;

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeNfcDeviceClient::MakeDeviceVisible,
                 base::Unretained(this),
                 record_push_delay),
      base::TimeDelta::FromMilliseconds(visibility_delay));
}

void FakeNfcDeviceClient::EndPairingSimulation() {
  if (!pairing_started_) {
    VLOG(1) << "No simulation started.";
    return;
  }
  if (device_visible_) {
    // Remove records, if they were added.
    if (!properties_->records.value().empty()) {
      FakeNfcRecordClient* record_client =
          static_cast<FakeNfcRecordClient*>(
              DBusThreadManager::Get()->GetNfcRecordClient());
      record_client->SetDeviceRecordsVisible(false);
    }
    // Remove the device.
    FOR_EACH_OBSERVER(Observer, observers_,
                      DeviceRemoved(dbus::ObjectPath(kDevicePath)));
    FakeNfcAdapterClient* adapter_client =
        static_cast<FakeNfcAdapterClient*>(
            DBusThreadManager::Get()->GetNfcAdapterClient());
    adapter_client->UnsetDevice(dbus::ObjectPath(kDevicePath));
    device_visible_ = false;
  }
  pairing_started_ = false;
}

void FakeNfcDeviceClient::EnableSimulationTimeout(int simulation_timeout) {
  simulation_timeout_ = simulation_timeout;
}

void FakeNfcDeviceClient::DisableSimulationTimeout() {
  simulation_timeout_ = -1;
}

void FakeNfcDeviceClient::SetRecords(
    const std::vector<dbus::ObjectPath>& record_paths) {
  if (!device_visible_) {
    VLOG(1) << "Device not visible.";
    return;
  }
  properties_->records.ReplaceValue(record_paths);
}

void FakeNfcDeviceClient::ClearRecords() {
  ObjectPathVector records;
  SetRecords(records);
}

void FakeNfcDeviceClient::OnPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  FOR_EACH_OBSERVER(NfcDeviceClient::Observer, observers_,
                    DevicePropertyChanged(object_path, property_name));
}

void FakeNfcDeviceClient::MakeDeviceVisible(int record_push_delay) {
  if (!pairing_started_) {
    VLOG(1) << "Device pairing was cancelled.";
    return;
  }
  device_visible_ = true;

  FakeNfcAdapterClient* adapter_client =
      static_cast<FakeNfcAdapterClient*>(
          DBusThreadManager::Get()->GetNfcAdapterClient());
  adapter_client->SetDevice(dbus::ObjectPath(kDevicePath));
  FOR_EACH_OBSERVER(Observer, observers_,
                    DeviceAdded(dbus::ObjectPath(kDevicePath)));

  if (record_push_delay < 0) {
    // Don't simulate record push. Instead, skip directly to the timeout step.
    if (simulation_timeout_ >= 0) {
      base::MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&FakeNfcDeviceClient::HandleSimulationTimeout,
                     base::Unretained(this)),
          base::TimeDelta::FromMilliseconds(simulation_timeout_));
    }
    return;
  }

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeNfcDeviceClient::MakeRecordsVisible,
                 base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(record_push_delay));
}

void FakeNfcDeviceClient::MakeRecordsVisible() {
  if (!pairing_started_) {
    VLOG(1) << "Pairing was cancelled";
    return;
  }
  DCHECK(device_visible_);
  FakeNfcRecordClient* record_client =
      static_cast<FakeNfcRecordClient*>(
          DBusThreadManager::Get()->GetNfcRecordClient());
  record_client->SetDeviceRecordsVisible(true);

  if (simulation_timeout_ < 0)
    return;

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeNfcDeviceClient::HandleSimulationTimeout,
                 base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(simulation_timeout_));
}

void FakeNfcDeviceClient::HandleSimulationTimeout() {
  if (simulation_timeout_ < 0) {
    VLOG(1) << "Simulation timeout was cancelled. Nothing to do.";
    return;
  }
  EndPairingSimulation();
}

}  // namespace chromeos
