// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_nfc_record_client.h"

#include "base/logging.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_nfc_device_client.h"
#include "chromeos/dbus/fake_nfc_tag_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// Gets and returns the value for |key| in |dictionary| as a string. If |key| is
// not found, returns an empty string.
std::string GetStringValue(const base::DictionaryValue& dictionary,
                           const std::string& key) {
  std::string value;
  bool result = dictionary.GetString(key, &value);

  // Simply return |value|. |value| will remain untouched if
  // base::DictionaryValue::GetString returns false.
  DCHECK(result || value.empty());
  return value;
}

// Gets and returns the value for |key| in |dictionary| as a double. If |key| is
// not found, returns 0.
double GetDoubleValue(const base::DictionaryValue& dictionary,
                      const std::string& key) {
  double value = 0;
  bool result = dictionary.GetDouble(key, &value);

  // Simply return |value|. |value| will remain untouched if
  // base::DictionaryValue::GetString returns false.
  DCHECK(result || !value);
  return value;
}

}  // namespace

const char FakeNfcRecordClient::kDeviceSmartPosterRecordPath[] =
    "/fake/device/record0";
const char FakeNfcRecordClient::kDeviceTextRecordPath[] =
    "/fake/device/record1";
const char FakeNfcRecordClient::kDeviceUriRecordPath[] = "/fake/device/record2";
const char FakeNfcRecordClient::kTagRecordPath[] = "/fake/tag/record0";

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

FakeNfcRecordClient::FakeNfcRecordClient()
    : device_records_visible_(false),
      tag_records_visible_(false) {
  VLOG(1) << "Creating FakeNfcRecordClient";

  device_smart_poster_record_properties_.reset(new Properties(
      base::Bind(&FakeNfcRecordClient::OnPropertyChanged,
                 base::Unretained(this),
                 dbus::ObjectPath(kDeviceSmartPosterRecordPath))));
  device_smart_poster_record_properties_->SetAllPropertiesReceivedCallback(
      base::Bind(&FakeNfcRecordClient::OnPropertiesReceived,
                 base::Unretained(this),
                 dbus::ObjectPath(kDeviceSmartPosterRecordPath)));

  device_text_record_properties_.reset(new Properties(
      base::Bind(&FakeNfcRecordClient::OnPropertyChanged,
                 base::Unretained(this),
                 dbus::ObjectPath(kDeviceTextRecordPath))));
  device_text_record_properties_->SetAllPropertiesReceivedCallback(
      base::Bind(&FakeNfcRecordClient::OnPropertiesReceived,
                 base::Unretained(this),
                 dbus::ObjectPath(kDeviceTextRecordPath)));

  device_uri_record_properties_.reset(new Properties(
      base::Bind(&FakeNfcRecordClient::OnPropertyChanged,
                 base::Unretained(this),
                 dbus::ObjectPath(kDeviceUriRecordPath))));
  device_uri_record_properties_->SetAllPropertiesReceivedCallback(
      base::Bind(&FakeNfcRecordClient::OnPropertiesReceived,
                 base::Unretained(this),
                 dbus::ObjectPath(kDeviceUriRecordPath)));

  tag_record_properties_.reset(new Properties(
      base::Bind(&FakeNfcRecordClient::OnPropertyChanged,
                 base::Unretained(this),
                 dbus::ObjectPath(kTagRecordPath))));
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
  if (device_records_visible_ &&
      device_path == dbus::ObjectPath(FakeNfcDeviceClient::kDevicePath)) {
    record_paths.push_back(dbus::ObjectPath(kDeviceSmartPosterRecordPath));
    record_paths.push_back(dbus::ObjectPath(kDeviceTextRecordPath));
    record_paths.push_back(dbus::ObjectPath(kDeviceUriRecordPath));
  }
  return record_paths;
}

std::vector<dbus::ObjectPath> FakeNfcRecordClient::GetRecordsForTag(
      const dbus::ObjectPath& tag_path) {
  std::vector<dbus::ObjectPath> record_paths;
  if (tag_records_visible_ && tag_path.value() == FakeNfcTagClient::kTagPath)
    record_paths.push_back(dbus::ObjectPath(kTagRecordPath));
  return record_paths;
}

FakeNfcRecordClient::Properties*
FakeNfcRecordClient::GetProperties(const dbus::ObjectPath& object_path) {
  if (device_records_visible_) {
    if (object_path.value() == kDeviceSmartPosterRecordPath)
      return device_smart_poster_record_properties_.get();
    if (object_path.value() == kDeviceTextRecordPath)
      return device_text_record_properties_.get();
    if (object_path.value() == kDeviceUriRecordPath)
      return device_uri_record_properties_.get();
    return NULL;
  }
  if (tag_records_visible_ && object_path.value() == kTagRecordPath)
      return tag_record_properties_.get();
  return NULL;
}

void FakeNfcRecordClient::SetDeviceRecordsVisible(bool visible) {
  if (device_records_visible_ == visible) {
    VLOG(1) << "Record visibility is already: " << visible;
    return;
  }
  FakeNfcDeviceClient* device_client = static_cast<FakeNfcDeviceClient*>(
      DBusThreadManager::Get()->GetNfcDeviceClient());
  if (!device_client->device_visible()) {
    VLOG(1) << "Cannot set records when device is not visible.";
    return;
  }
  if (!visible) {
    device_client->ClearRecords();
    FOR_EACH_OBSERVER(
        NfcRecordClient::Observer, observers_,
        RecordRemoved(dbus::ObjectPath(kDeviceSmartPosterRecordPath)));
    FOR_EACH_OBSERVER(NfcRecordClient::Observer, observers_,
                      RecordRemoved(dbus::ObjectPath(kDeviceTextRecordPath)));
    FOR_EACH_OBSERVER(NfcRecordClient::Observer, observers_,
                      RecordRemoved(dbus::ObjectPath(kDeviceUriRecordPath)));
    device_records_visible_ = visible;
    return;
  }
  device_records_visible_ = visible;
  std::vector<dbus::ObjectPath> record_paths =
      GetRecordsForDevice(
          dbus::ObjectPath(FakeNfcDeviceClient::kDevicePath));
  device_client->SetRecords(record_paths);

  // Reassign each property and send signals.
  FOR_EACH_OBSERVER(
      NfcRecordClient::Observer, observers_,
      RecordAdded(dbus::ObjectPath(kDeviceSmartPosterRecordPath)));
  device_smart_poster_record_properties_->type.ReplaceValue(
      nfc_record::kTypeSmartPoster);
  device_smart_poster_record_properties_->uri.ReplaceValue(
      "http://fake.uri0.fake");
  device_smart_poster_record_properties_->mime_type.ReplaceValue("text/fake");
  device_smart_poster_record_properties_->size.ReplaceValue(128);
  device_smart_poster_record_properties_->
      representation.ReplaceValue("Fake Title");
  device_smart_poster_record_properties_->encoding.ReplaceValue(
      nfc_record::kEncodingUtf16);
  device_smart_poster_record_properties_->language.ReplaceValue("en");
  OnPropertiesReceived(dbus::ObjectPath(kDeviceSmartPosterRecordPath));

  FOR_EACH_OBSERVER(NfcRecordClient::Observer, observers_,
                    RecordAdded(dbus::ObjectPath(kDeviceTextRecordPath)));
  device_text_record_properties_->type.ReplaceValue(nfc_record::kTypeText);
  device_text_record_properties_->representation.ReplaceValue(
      "Kablosuz \xC4\xb0leti\xC5\x9fim");
  device_text_record_properties_->encoding.ReplaceValue(
      nfc_record::kEncodingUtf8);
  device_text_record_properties_->language.ReplaceValue("tr");
  OnPropertiesReceived(dbus::ObjectPath(kDeviceTextRecordPath));

  FOR_EACH_OBSERVER(NfcRecordClient::Observer, observers_,
                    RecordAdded(dbus::ObjectPath(kDeviceUriRecordPath)));
  device_uri_record_properties_->type.ReplaceValue(nfc_record::kTypeUri);
  device_uri_record_properties_->uri.ReplaceValue("file://some/fake/path");
  device_uri_record_properties_->mime_type.ReplaceValue("text/fake");
  device_uri_record_properties_->size.ReplaceValue(512);
  OnPropertiesReceived(dbus::ObjectPath(kDeviceUriRecordPath));
}

void FakeNfcRecordClient::SetTagRecordsVisible(bool visible) {
  if (tag_records_visible_ == visible) {
    VLOG(1) << "Record visibility is already: " << visible;
    return;
  }
  FakeNfcTagClient* tag_client = static_cast<FakeNfcTagClient*>(
      DBusThreadManager::Get()->GetNfcTagClient());
  if (!tag_client->tag_visible()) {
    VLOG(1) << "Cannot set records when tag is not visible.";
    return;
  }
  if (!visible) {
    tag_client->ClearRecords();
    FOR_EACH_OBSERVER(NfcRecordClient::Observer, observers_,
                      RecordRemoved(dbus::ObjectPath(kTagRecordPath)));
    tag_records_visible_ = visible;
    return;
  }
  tag_records_visible_ = visible;
  std::vector<dbus::ObjectPath> record_paths =
    GetRecordsForTag(dbus::ObjectPath(FakeNfcTagClient::kTagPath));
  tag_client->SetRecords(record_paths);

  // Reassign each property to its current value to trigger a property change
  // signal.
  FOR_EACH_OBSERVER(NfcRecordClient::Observer, observers_,
                    RecordAdded(dbus::ObjectPath(kTagRecordPath)));
  tag_record_properties_->type.ReplaceValue(
      tag_record_properties_->type.value());
  tag_record_properties_->representation.ReplaceValue(
      tag_record_properties_->representation.value());
  tag_record_properties_->encoding.ReplaceValue(
      tag_record_properties_->encoding.value());
  tag_record_properties_->language.ReplaceValue(
      tag_record_properties_->language.value());
  tag_record_properties_->uri.ReplaceValue(
      tag_record_properties_->uri.value());
  tag_record_properties_->mime_type.ReplaceValue(
      tag_record_properties_->mime_type.value());
  tag_record_properties_->size.ReplaceValue(
      tag_record_properties_->size.value());
  tag_record_properties_->action.ReplaceValue(
      tag_record_properties_->action.value());
  OnPropertiesReceived(dbus::ObjectPath(kTagRecordPath));
}

bool FakeNfcRecordClient::WriteTagRecord(
    const base::DictionaryValue& attributes) {
  if (attributes.empty())
    return false;

  tag_record_properties_->type.ReplaceValue(
      GetStringValue(attributes, nfc_record::kTypeProperty));
  tag_record_properties_->encoding.ReplaceValue(
      GetStringValue(attributes, nfc_record::kEncodingProperty));
  tag_record_properties_->language.ReplaceValue(
      GetStringValue(attributes, nfc_record::kLanguageProperty));
  tag_record_properties_->representation.ReplaceValue(
      GetStringValue(attributes, nfc_record::kRepresentationProperty));
  tag_record_properties_->uri.ReplaceValue(
      GetStringValue(attributes, nfc_record::kUriProperty));
  tag_record_properties_->mime_type.ReplaceValue(
      GetStringValue(attributes, nfc_record::kMimeTypeProperty));
  tag_record_properties_->action.ReplaceValue(
      GetStringValue(attributes, nfc_record::kActionProperty));
  tag_record_properties_->size.ReplaceValue(static_cast<uint32>(
      GetDoubleValue(attributes, nfc_record::kSizeProperty)));

  SetTagRecordsVisible(false);
  SetTagRecordsVisible(true);

  return true;
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
