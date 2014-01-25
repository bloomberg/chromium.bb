// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_nfc_adapter_client.h"

#include "base/logging.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_nfc_device_client.h"
#include "chromeos/dbus/fake_nfc_tag_client.h"
#include "chromeos/dbus/nfc_client_helpers.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

using nfc_client_helpers::ObjectPathVector;

const char FakeNfcAdapterClient::kAdapterPath0[] = "/fake/nfc0";
const char FakeNfcAdapterClient::kAdapterPath1[] = "/fake/nfc1";

FakeNfcAdapterClient::Properties::Properties(
    const PropertyChangedCallback& callback)
    : NfcAdapterClient::Properties(NULL, callback) {
}

FakeNfcAdapterClient::Properties::~Properties() {
}

void FakeNfcAdapterClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  VLOG(1) << "Get " << property->name();
  callback.Run(false);
}

void FakeNfcAdapterClient::Properties::GetAll() {
  VLOG(1) << "GetAll";
}

void FakeNfcAdapterClient::Properties::Set(
    dbus::PropertyBase* property,
    dbus::PropertySet::SetCallback callback) {
  VLOG(1) << "Set " << property->name();
  if (property->name() != powered.name()) {
    callback.Run(false);
    return;
  }

  // Cannot set the power if currently polling.
  if (polling.value()) {
    LOG(ERROR) << "Cannot set power while polling.";
    callback.Run(false);
    return;
  }

  // Cannot set power if there is a device or a tag that is currently
  // "paired".
  if (!devices.value().empty() || !tags.value().empty()) {
    LOG(ERROR) << "Cannot set power while the device is paired.";
    callback.Run(false);
    return;
  }

  // Obtain the cached "set value" and send a property changed signal only if
  // its value is different from the current value of the property.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  property->AppendSetValueToWriter(&writer);
  dbus::MessageReader reader(response.get());
  bool set_value = false;
  if (!reader.PopVariantOfBool(&set_value) || set_value == powered.value()) {
    LOG(WARNING) << "Property has not changed.";
    callback.Run(false);
    return;
  }
  property->ReplaceValueWithSetValue();
  callback.Run(true);
}

FakeNfcAdapterClient::FakeNfcAdapterClient()
    : present_(true),
      second_present_(false),
      start_pairing_on_poll_(true),
      device_pairing_(false) {
  VLOG(1) << "Creating FakeNfcAdapterClient";

  std::vector<std::string> protocols;
  protocols.push_back(nfc_common::kProtocolFelica);
  protocols.push_back(nfc_common::kProtocolMifare);
  protocols.push_back(nfc_common::kProtocolJewel);
  protocols.push_back(nfc_common::kProtocolIsoDep);
  protocols.push_back(nfc_common::kProtocolNfcDep);

  properties_.reset(new Properties(base::Bind(
      &FakeNfcAdapterClient::OnPropertyChanged,
      base::Unretained(this),
      dbus::ObjectPath(kAdapterPath0))));
  properties_->protocols.ReplaceValue(protocols);

  second_properties_.reset(new Properties(base::Bind(
      &FakeNfcAdapterClient::OnPropertyChanged,
      base::Unretained(this),
      dbus::ObjectPath(kAdapterPath1))));
  second_properties_->protocols.ReplaceValue(protocols);
}

FakeNfcAdapterClient::~FakeNfcAdapterClient() {
}

void FakeNfcAdapterClient::Init(dbus::Bus* bus) {
}

void FakeNfcAdapterClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeNfcAdapterClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::vector<dbus::ObjectPath> FakeNfcAdapterClient::GetAdapters() {
  std::vector<dbus::ObjectPath> object_paths;
  if (present_)
    object_paths.push_back(dbus::ObjectPath(kAdapterPath0));
  if (second_present_)
    object_paths.push_back(dbus::ObjectPath(kAdapterPath1));
  return object_paths;
}

FakeNfcAdapterClient::Properties*
FakeNfcAdapterClient::GetProperties(const dbus::ObjectPath& object_path) {
  if (object_path == dbus::ObjectPath(kAdapterPath0))
    return properties_.get();
  if (object_path == dbus::ObjectPath(kAdapterPath1))
    return second_properties_.get();
  return NULL;
}

void FakeNfcAdapterClient::StartPollLoop(
    const dbus::ObjectPath& object_path,
    const std::string& mode,
    const base::Closure& callback,
    const nfc_client_helpers::ErrorCallback& error_callback) {
  VLOG(1) << "FakeNfcAdapterClient::StartPollLoop";
  if (object_path != dbus::ObjectPath(kAdapterPath0)) {
    error_callback.Run(nfc_client_helpers::kNoResponseError, "");
    return;
  }
  if (!properties_->powered.value()) {
    error_callback.Run(nfc_error::kFailed, "Adapter not powered.");
    return;
  }
  if (properties_->polling.value()) {
    error_callback.Run(nfc_error::kFailed, "Already polling.");
    return;
  }
  if (!properties_->devices.value().empty() ||
      !properties_->tags.value().empty()) {
    error_callback.Run(nfc_error::kFailed, "Adapter busy.");
    return;
  }
  properties_->polling.ReplaceValue(true);
  properties_->mode.ReplaceValue(mode);
  callback.Run();

  if (!start_pairing_on_poll_)
    return;

  if (device_pairing_) {
    FakeNfcDeviceClient* device_client =
        static_cast<FakeNfcDeviceClient*>(
            DBusThreadManager::Get()->GetNfcDeviceClient());
    device_client->BeginPairingSimulation(3000, 2000);
  } else {
    FakeNfcTagClient* tag_client =
        static_cast<FakeNfcTagClient*>(
            DBusThreadManager::Get()->GetNfcTagClient());
    tag_client->BeginPairingSimulation(2000);
  }
  device_pairing_ = !device_pairing_;
}

void FakeNfcAdapterClient::StopPollLoop(
    const dbus::ObjectPath& object_path,
    const base::Closure& callback,
    const nfc_client_helpers::ErrorCallback& error_callback) {
  VLOG(1) << "FakeNfcAdapterClient::StopPollLoop.";
  if (object_path != dbus::ObjectPath(kAdapterPath0)) {
    error_callback.Run(nfc_client_helpers::kNoResponseError, "");
    return;
  }
  if (!properties_->polling.value()) {
    error_callback.Run("org.neard.Error.Failed", "Not polling.");
    return;
  }
  FakeNfcDeviceClient* device_client =
      static_cast<FakeNfcDeviceClient*>(
          DBusThreadManager::Get()->GetNfcDeviceClient());
  device_client->EndPairingSimulation();
  FakeNfcTagClient* tag_client =
      static_cast<FakeNfcTagClient*>(
          DBusThreadManager::Get()->GetNfcTagClient());
  tag_client->EndPairingSimulation();
  properties_->polling.ReplaceValue(false);
  callback.Run();
}

void FakeNfcAdapterClient::SetAdapterPresent(bool present) {
  if (present == present_)
    return;
  present_ = present;
  if (present_) {
    FOR_EACH_OBSERVER(NfcAdapterClient::Observer, observers_,
                      AdapterAdded(dbus::ObjectPath(kAdapterPath0)));
  } else {
    FOR_EACH_OBSERVER(NfcAdapterClient::Observer, observers_,
                      AdapterRemoved(dbus::ObjectPath(kAdapterPath0)));
  }
}

void FakeNfcAdapterClient::SetSecondAdapterPresent(bool present) {
  if (present == second_present_)
    return;
  second_present_ = present;
  if (present_) {
    FOR_EACH_OBSERVER(NfcAdapterClient::Observer, observers_,
                      AdapterAdded(dbus::ObjectPath(kAdapterPath1)));
  } else {
    FOR_EACH_OBSERVER(NfcAdapterClient::Observer, observers_,
                      AdapterRemoved(dbus::ObjectPath(kAdapterPath1)));
  }
}

void FakeNfcAdapterClient::SetDevice(const dbus::ObjectPath& device_path) {
  LOG(INFO) << "Add device path to the fake adapter: " << device_path.value();
  if (!properties_->polling.value()) {
    LOG(ERROR) << "Adapter not polling, cannot set device.";
    return;
  }
  const ObjectPathVector& devices(properties_->devices.value());
  for (ObjectPathVector::const_iterator iter = devices.begin();
       iter != devices.end(); ++iter) {
    if (*iter == device_path) {
      LOG(WARNING) << "Device path already in list of devices.";
      return;
    }
  }
  // Mark as not polling.
  properties_->polling.ReplaceValue(false);

  ObjectPathVector new_devices = devices;
  new_devices.push_back(device_path);
  properties_->devices.ReplaceValue(new_devices);
}

void FakeNfcAdapterClient::SetTag(const dbus::ObjectPath& tag_path) {
  LOG(INFO) << "Add tag path to the fake adapter: " << tag_path.value();
  if (!properties_->polling.value()) {
    LOG(ERROR) << "Adapter not polling, cannot set tag.";
    return;
  }
  const ObjectPathVector& tags(properties_->tags.value());
  for (ObjectPathVector::const_iterator iter = tags.begin();
       iter != tags.end(); ++iter) {
    if (*iter == tag_path) {
      LOG(WARNING) << "Tag path already in list of tags.";
      return;
    }
  }
  // Mark as not polling.
  properties_->polling.ReplaceValue(false);

  ObjectPathVector new_tags = tags;
  new_tags.push_back(tag_path);
  properties_->tags.ReplaceValue(new_tags);
}

void FakeNfcAdapterClient::UnsetDevice(const dbus::ObjectPath& device_path) {
  LOG(INFO) << "Remove device path from the fake adapter: "
            << device_path.value();
  ObjectPathVector new_devices = properties_->devices.value();
  for (ObjectPathVector::iterator iter = new_devices.begin();
       iter != new_devices.end(); ++iter) {
    if (*iter == device_path) {
      new_devices.erase(iter);
      properties_->devices.ReplaceValue(new_devices);

      // Mark as polling.
      DCHECK(!properties_->polling.value());
      properties_->polling.ReplaceValue(true);
      return;
    }
  }
  LOG(WARNING) << "Device path not in list of devices.";
}

void FakeNfcAdapterClient::UnsetTag(const dbus::ObjectPath& tag_path) {
  LOG(INFO) << "Remove tag path from the fake adapter: " << tag_path.value();
  ObjectPathVector new_tags = properties_->tags.value();
  for (ObjectPathVector::iterator iter = new_tags.begin();
       iter != new_tags.end(); ++iter) {
    if (*iter == tag_path) {
      new_tags.erase(iter);
      properties_->tags.ReplaceValue(new_tags);

      // Mark as polling.
      DCHECK(!properties_->polling.value());
      properties_->polling.ReplaceValue(true);
      return;
    }
  }
  LOG(WARNING) << "Tag path not in list of tags.";
}

void FakeNfcAdapterClient::EnablePairingOnPoll(bool enabled) {
  start_pairing_on_poll_ = enabled;
}

void FakeNfcAdapterClient::OnPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  FOR_EACH_OBSERVER(NfcAdapterClient::Observer, observers_,
                    AdapterPropertyChanged(object_path, property_name));
}

}  // namespace chromeos
