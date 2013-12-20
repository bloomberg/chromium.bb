// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_nfc_record_client.h"

#include "base/logging.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_nfc_device_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

const char FakeNfcRecordClient::kSmartPosterRecordPath[] = "/fake/record0";
const char FakeNfcRecordClient::kTextRecordPath[] = "/fake/record1";
const char FakeNfcRecordClient::kUriRecordPath[] = "/fake/record2";

FakeNfcRecordClient::Properties::Properties(
    const PropertyChangedCallback& callback)
    : NfcRecordClient::Properties(NULL, callback) {
}

FakeNfcRecordClient::Properties::~Properties() {
}

void FakeNfcRecordClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  VLOG(1) << "Get " << property->name();
  callback.Run(false);
}

void FakeNfcRecordClient::Properties::GetAll() {
  VLOG(1) << "GetAll";
  if (!on_get_all_callback().is_null())
    on_get_all_callback().Run();
}

void FakeNfcRecordClient::Properties::Set(
    dbus::PropertyBase* property,
    dbus::PropertySet::SetCallback callback) {
  VLOG(1) << "Set " << property->name();
  callback.Run(false);
}

FakeNfcRecordClient::FakeNfcRecordClient() : records_visible_(false) {
  VLOG(1) << "Creating FakeNfcRecordClient";
  smart_poster_record_properties_.reset(new Properties(
      base::Bind(&FakeNfcRecordClient::OnPropertyChanged,
                 base::Unretained(this),
                 dbus::ObjectPath(kSmartPosterRecordPath))));
  smart_poster_record_properties_->SetAllPropertiesReceivedCallback(
      base::Bind(&FakeNfcRecordClient::OnPropertiesReceived,
                 base::Unretained(this),
                 dbus::ObjectPath(kSmartPosterRecordPath)));

  text_record_properties_.reset(new Properties(
      base::Bind(&FakeNfcRecordClient::OnPropertyChanged,
                 base::Unretained(this),
                 dbus::ObjectPath(kTextRecordPath))));
  text_record_properties_->SetAllPropertiesReceivedCallback(
      base::Bind(&FakeNfcRecordClient::OnPropertiesReceived,
                 base::Unretained(this),
                 dbus::ObjectPath(kTextRecordPath)));

  uri_record_properties_.reset(new Properties(
      base::Bind(&FakeNfcRecordClient::OnPropertyChanged,
                 base::Unretained(this),
                 dbus::ObjectPath(kUriRecordPath))));
  uri_record_properties_->SetAllPropertiesReceivedCallback(
      base::Bind(&FakeNfcRecordClient::OnPropertiesReceived,
                 base::Unretained(this),
                 dbus::ObjectPath(kUriRecordPath)));
}

FakeNfcRecordClient::~FakeNfcRecordClient() {
}

void FakeNfcRecordClient::Init(dbus::Bus* bus) {
}

void FakeNfcRecordClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeNfcRecordClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::vector<dbus::ObjectPath> FakeNfcRecordClient::GetRecordsForDevice(
      const dbus::ObjectPath& device_path) {
  std::vector<dbus::ObjectPath> record_paths;
  if (records_visible_ &&
      device_path == dbus::ObjectPath(FakeNfcDeviceClient::kDevicePath)) {
    record_paths.push_back(dbus::ObjectPath(kSmartPosterRecordPath));
    record_paths.push_back(dbus::ObjectPath(kTextRecordPath));
    record_paths.push_back(dbus::ObjectPath(kUriRecordPath));
  }
  return record_paths;
}

FakeNfcRecordClient::Properties*
FakeNfcRecordClient::GetProperties(const dbus::ObjectPath& object_path) {
  if (!records_visible_)
    return NULL;
  if (object_path.value() == kSmartPosterRecordPath)
    return smart_poster_record_properties_.get();
  if (object_path.value() == kTextRecordPath)
    return text_record_properties_.get();
  if (object_path.value() == kUriRecordPath)
    return uri_record_properties_.get();
  return NULL;
}

void FakeNfcRecordClient::SetRecordsVisible(bool visible) {
  if (records_visible_ == visible) {
    VLOG(1) << "Record visibility is already: " << visible;
    return;
  }
  FakeNfcDeviceClient* device_client = static_cast<FakeNfcDeviceClient*>(
      DBusThreadManager::Get()->GetNfcDeviceClient());
  if (!device_client->device_visible()) {
    VLOG(1) << "Cannot set records when device is not visible.";
    return;
  }
  if (visible) {
    records_visible_ = visible;
    std::vector<dbus::ObjectPath> record_paths =
        GetRecordsForDevice(
            dbus::ObjectPath(FakeNfcDeviceClient::kDevicePath));
    device_client->SetRecords(record_paths);

    // Reassign each property and send signals.
    FOR_EACH_OBSERVER(NfcRecordClient::Observer, observers_,
                      RecordAdded(dbus::ObjectPath(kSmartPosterRecordPath)));
    smart_poster_record_properties_->type.ReplaceValue(
        nfc_record::kTypeSmartPoster);
    smart_poster_record_properties_->uri.ReplaceValue(
        "http://www.fake-uri0.com");
    smart_poster_record_properties_->mime_type.ReplaceValue("text/fake");
    smart_poster_record_properties_->size.ReplaceValue(128);
    smart_poster_record_properties_->representation.ReplaceValue("Fake Title");
    smart_poster_record_properties_->encoding.ReplaceValue(
        nfc_record::kEncodingUtf16);
    smart_poster_record_properties_->language.ReplaceValue("en");
    OnPropertiesReceived(dbus::ObjectPath(kSmartPosterRecordPath));

    FOR_EACH_OBSERVER(NfcRecordClient::Observer, observers_,
                      RecordAdded(dbus::ObjectPath(kTextRecordPath)));
    text_record_properties_->type.ReplaceValue(nfc_record::kTypeText);
    text_record_properties_->representation.ReplaceValue(
        "Sahte Ba\xC5\x9fl\xC4\xB1k");
    text_record_properties_->encoding.ReplaceValue(
        nfc_record::kEncodingUtf8);
    text_record_properties_->language.ReplaceValue("tr");
    OnPropertiesReceived(dbus::ObjectPath(kTextRecordPath));

    FOR_EACH_OBSERVER(NfcRecordClient::Observer, observers_,
                      RecordAdded(dbus::ObjectPath(kUriRecordPath)));
    uri_record_properties_->type.ReplaceValue(nfc_record::kTypeUri);
    uri_record_properties_->uri.ReplaceValue("file://some/fake/path");
    uri_record_properties_->mime_type.ReplaceValue("text/fake");
    uri_record_properties_->size.ReplaceValue(512);
    OnPropertiesReceived(dbus::ObjectPath(kUriRecordPath));
  } else {
    device_client->ClearRecords();
    FOR_EACH_OBSERVER(NfcRecordClient::Observer, observers_,
                      RecordRemoved(dbus::ObjectPath(kSmartPosterRecordPath)));
    FOR_EACH_OBSERVER(NfcRecordClient::Observer, observers_,
                      RecordRemoved(dbus::ObjectPath(kTextRecordPath)));
    FOR_EACH_OBSERVER(NfcRecordClient::Observer, observers_,
                      RecordRemoved(dbus::ObjectPath(kUriRecordPath)));
    records_visible_ = visible;
  }
}

void FakeNfcRecordClient::OnPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  FOR_EACH_OBSERVER(NfcRecordClient::Observer, observers_,
                    RecordPropertyChanged(object_path, property_name));
}

void FakeNfcRecordClient::OnPropertiesReceived(
    const dbus::ObjectPath& object_path) {
  FOR_EACH_OBSERVER(NfcRecordClient::Observer, observers_,
                    RecordPropertiesReceived(object_path));
}

}  // namespace chromeos
