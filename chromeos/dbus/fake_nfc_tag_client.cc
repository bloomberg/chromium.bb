// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_nfc_tag_client.h"

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

const char FakeNfcTagClient::kTagPath[] = "/fake/tag0";
const int FakeNfcTagClient::kDefaultSimulationTimeoutMilliseconds = 20000;

FakeNfcTagClient::Properties::Properties(
    const PropertyChangedCallback& callback)
    : NfcTagClient::Properties(NULL, callback) {
}

FakeNfcTagClient::Properties::~Properties() {
}

void FakeNfcTagClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  VLOG(1) << "Get " << property->name();
  callback.Run(false);
}

void FakeNfcTagClient::Properties::GetAll() {
  VLOG(1) << "GetAll";
}

void FakeNfcTagClient::Properties::Set(
    dbus::PropertyBase* property,
    dbus::PropertySet::SetCallback callback) {
  VLOG(1) << "Set " << property->name();
  callback.Run(false);
}

FakeNfcTagClient::FakeNfcTagClient()
    : pairing_started_(false),
      tag_visible_(false),
      simulation_timeout_(kDefaultSimulationTimeoutMilliseconds) {
  VLOG(1) << "Creating FakeNfcTagClient";
  properties_.reset(new Properties(
      base::Bind(&FakeNfcTagClient::OnPropertyChanged,
                 base::Unretained(this),
                 dbus::ObjectPath(kTagPath))));
}

FakeNfcTagClient::~FakeNfcTagClient() {
}

void FakeNfcTagClient::Init(dbus::Bus* bus) {
}

void FakeNfcTagClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeNfcTagClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::vector<dbus::ObjectPath> FakeNfcTagClient::GetTagsForAdapter(
    const dbus::ObjectPath& adapter_path) {
  std::vector<dbus::ObjectPath> tag_paths;
  if (tag_visible_ &&
      adapter_path.value() == FakeNfcAdapterClient::kAdapterPath0)
    tag_paths.push_back(dbus::ObjectPath(kTagPath));
  return tag_paths;
}

FakeNfcTagClient::Properties*
FakeNfcTagClient::GetProperties(const dbus::ObjectPath& object_path) {
  if (!tag_visible_)
    return NULL;
  return properties_.get();
}

void FakeNfcTagClient::Write(
    const dbus::ObjectPath& object_path,
    const base::DictionaryValue& attributes,
    const base::Closure& callback,
    const nfc_client_helpers::ErrorCallback& error_callback) {
  VLOG(1) << "FakeNfcTagClient::Write called. Nothing happened.";

  if (!tag_visible_ || object_path.value() != kTagPath) {
    LOG(ERROR) << "No such tag: " << object_path.value();
    error_callback.Run(nfc_error::kDoesNotExist, "No such tag.");
    return;
  }

  FakeNfcRecordClient* record_client = static_cast<FakeNfcRecordClient*>(
      DBusThreadManager::Get()->GetNfcRecordClient());
  if (!record_client->WriteTagRecord(attributes)) {
    LOG(ERROR) << "Failed to tag: " << object_path.value();
    error_callback.Run(nfc_error::kFailed, "Failed.");
    return;
  }

  // Success!
  callback.Run();
}

void FakeNfcTagClient::BeginPairingSimulation(int visibility_delay) {
  if (pairing_started_) {
    VLOG(1) << "Simulation already started.";
    return;
  }
  DCHECK(!tag_visible_);
  DCHECK(visibility_delay >= 0);

  pairing_started_ = true;

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeNfcTagClient::MakeTagVisible,
                 base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(visibility_delay));
}

void FakeNfcTagClient::EndPairingSimulation() {
  if (!pairing_started_) {
    VLOG(1) << "No simulation started.";
    return;
  }

  pairing_started_ = false;
  if (!tag_visible_)
    return;

  // Remove records, if they were added.
  if (!properties_->records.value().empty()) {
    FakeNfcRecordClient* record_client =
        static_cast<FakeNfcRecordClient*>(
            DBusThreadManager::Get()->GetNfcRecordClient());
    record_client->SetTagRecordsVisible(false);
  }
  // Remove the tag.
  FOR_EACH_OBSERVER(Observer, observers_,
                    TagRemoved(dbus::ObjectPath(kTagPath)));
  FakeNfcAdapterClient* adapter_client =
      static_cast<FakeNfcAdapterClient*>(
          DBusThreadManager::Get()->GetNfcAdapterClient());
  adapter_client->UnsetTag(dbus::ObjectPath(kTagPath));
  tag_visible_ = false;
}

void FakeNfcTagClient::EnableSimulationTimeout(int simulation_timeout) {
  DCHECK(simulation_timeout >= 0);
  simulation_timeout_ = simulation_timeout;
}

void FakeNfcTagClient::DisableSimulationTimeout() {
  simulation_timeout_ = -1;
}

void FakeNfcTagClient::SetRecords(
    const std::vector<dbus::ObjectPath>& record_paths) {
  if (!tag_visible_) {
    VLOG(1) << "Tag not visible.";
    return;
  }
  properties_->records.ReplaceValue(record_paths);
}

void FakeNfcTagClient::ClearRecords() {
  ObjectPathVector records;
  SetRecords(records);
}

void FakeNfcTagClient::OnPropertyChanged(const dbus::ObjectPath& object_path,
                                         const std::string& property_name) {
  FOR_EACH_OBSERVER(Observer, observers_,
                    TagPropertyChanged(object_path, property_name));
}

void FakeNfcTagClient::MakeTagVisible() {
  if (!pairing_started_) {
    VLOG(1) << "Tag pairing was cancelled.";
    return;
  }
  tag_visible_ = true;

  // Set the tag properties.
  FakeNfcAdapterClient* adapter_client =
      static_cast<FakeNfcAdapterClient*>(
          DBusThreadManager::Get()->GetNfcAdapterClient());
  adapter_client->SetTag(dbus::ObjectPath(kTagPath));
  FOR_EACH_OBSERVER(Observer, observers_,
                    TagAdded(dbus::ObjectPath(kTagPath)));

  properties_->type.ReplaceValue(nfc_tag::kTagType2);
  properties_->protocol.ReplaceValue(nfc_common::kProtocolNfcDep);
  properties_->read_only.ReplaceValue(false);
  FOR_EACH_OBSERVER(Observer, observers_,
                    TagPropertiesReceived(dbus::ObjectPath(kTagPath)));

  if (simulation_timeout_ >= 0) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeNfcTagClient::HandleSimulationTimeout,
                   base::Unretained(this)),
        base::TimeDelta::FromMilliseconds(simulation_timeout_));
    return;
  }
}

void FakeNfcTagClient::HandleSimulationTimeout() {
  if (simulation_timeout_ < 0) {
    VLOG(1) << "Simulation timeout was cancelled. Nothing to do.";
    return;
  }
  EndPairingSimulation();
}

}  // namespace chromeos
