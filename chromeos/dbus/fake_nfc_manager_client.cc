// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_nfc_manager_client.h"

#include "base/bind.h"
#include "base/logging.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

const char FakeNfcManagerClient::kDefaultAdapterPath[] = "/fake/nfc0";

FakeNfcManagerClient::Properties::Properties(
    const PropertyChangedCallback& callback)
    : NfcManagerClient::Properties(NULL, callback) {
}

FakeNfcManagerClient::Properties::~Properties() {
}

void FakeNfcManagerClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  VLOG(1) << "Get " << property->name();
  callback.Run(false);
}

void FakeNfcManagerClient::Properties::GetAll() {
  VLOG(1) << "GetAll";
}

void FakeNfcManagerClient::Properties::Set(
    dbus::PropertyBase* property,
    dbus::PropertySet::SetCallback callback) {
  VLOG(1) << "Set " << property->name();
  callback.Run(false);
}

FakeNfcManagerClient::FakeNfcManagerClient() {
  properties_.reset(new Properties(base::Bind(
      &FakeNfcManagerClient::OnPropertyChanged, base::Unretained(this))));
  AddAdapter(kDefaultAdapterPath);
}

FakeNfcManagerClient::~FakeNfcManagerClient() {
}

void FakeNfcManagerClient::Init(dbus::Bus* bus) {
}

void FakeNfcManagerClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeNfcManagerClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

FakeNfcManagerClient::Properties* FakeNfcManagerClient::GetProperties() {
  return properties_.get();
}

void FakeNfcManagerClient::AddAdapter(const std::string& adapter_path) {
  VLOG(1) << "Adding NFC adapter: " << adapter_path;
  dbus::ObjectPath new_adapter(adapter_path);
  std::pair<std::set<dbus::ObjectPath>::iterator, bool> result =
      adapters_.insert(new_adapter);
  if (!result.second) {
    VLOG(1) << "Adapter \"" << adapter_path << "\" already exists.";
    return;
  }
  // Create a vector containing all object paths in the set |adapters_|. This
  // will copy all members of |adapters_| to |adapters|.
  std::vector<dbus::ObjectPath> adapters(adapters_.begin(), adapters_.end());
  properties_->adapters.ReplaceValue(adapters);
  FOR_EACH_OBSERVER(Observer, observers_, AdapterAdded(new_adapter));
}

void FakeNfcManagerClient::RemoveAdapter(const std::string& adapter_path) {
  VLOG(1) << "Removing NFC adapter: " << adapter_path;
  dbus::ObjectPath to_remove(adapter_path);
  if (adapters_.erase(to_remove) == 0) {
    VLOG(1) << "No such adapter: \"" << adapter_path << "\"";
    return;
  }
  std::vector<dbus::ObjectPath> adapters(adapters_.begin(), adapters_.end());
  properties_->adapters.ReplaceValue(adapters);
  FOR_EACH_OBSERVER(Observer, observers_, AdapterRemoved(to_remove));
}

void FakeNfcManagerClient::OnPropertyChanged(
    const std::string& property_name) {
  FOR_EACH_OBSERVER(Observer, observers_,
                    ManagerPropertyChanged(property_name));
}

}  // namespace chromeos
