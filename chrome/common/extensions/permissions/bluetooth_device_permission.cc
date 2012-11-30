// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/bluetooth_device_permission.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/permissions/permissions_info.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char* kSeparator = "|";

}  // namespace

namespace extensions {

BluetoothDevicePermission::BluetoothDevicePermission(
    const APIPermissionInfo* info) : APIPermission(info) {
}

BluetoothDevicePermission::~BluetoothDevicePermission() {
}

void BluetoothDevicePermission::AddDevicesFromString(
    const std::string &devices_string) {
  std::vector<std::string> devices;
  Tokenize(devices_string, kSeparator, &devices);
  for (std::vector<std::string>::const_iterator i = devices.begin();
      i != devices.end(); ++i) {
    devices_.insert(*i);
  }
}

std::string BluetoothDevicePermission::ToString() const {
  std::vector<std::string> parts;
  parts.push_back(name());
  for (std::set<std::string>::const_iterator i = devices_.begin();
      i != devices_.end(); ++i) {
    parts.push_back(*i);
  }
  return JoinString(parts, kSeparator);
}

bool BluetoothDevicePermission::ManifestEntryForbidden() const {
  return true;
}

bool BluetoothDevicePermission::HasMessages() const {
  return !devices_.empty();
}

PermissionMessages BluetoothDevicePermission::GetMessages() const {
  DCHECK(HasMessages());
  PermissionMessages result;

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter =
      device::BluetoothAdapterFactory::DefaultAdapter();

  for (std::set<std::string>::const_iterator i = devices_.begin();
      i != devices_.end(); ++i) {

    string16 device_identifier;
    if (bluetooth_adapter) {
      device::BluetoothDevice* device = bluetooth_adapter->GetDevice(*i);
      if (device)
        device_identifier = device->GetName();
    }

    if (device_identifier.length() == 0) {
      UTF8ToUTF16(i->c_str(), i->length(), &device_identifier);
    }

    result.push_back(PermissionMessage(
          PermissionMessage::kBluetoothDevice,
          l10n_util::GetStringFUTF16(
              IDS_EXTENSION_PROMPT_WARNING_BLUETOOTH_DEVICE,
              device_identifier)));
  }

  return result;
}

bool BluetoothDevicePermission::Check(
    const APIPermission::CheckParam* param) const {
  const CheckParam* bluetooth_device_parameter =
      static_cast<const CheckParam*>(param);
  for (std::set<std::string>::const_iterator i = devices_.begin();
      i != devices_.end(); ++i) {
    if (*i == bluetooth_device_parameter->device_address)
      return true;
  }
  return false;
}

bool BluetoothDevicePermission::Contains(const APIPermission* rhs) const {
  CHECK(rhs->info() == info());
  const BluetoothDevicePermission* perm =
      static_cast<const BluetoothDevicePermission*>(rhs);
  return std::includes(
      devices_.begin(), devices_.end(),
      perm->devices_.begin(), perm->devices_.end());
}

bool BluetoothDevicePermission::Equal(const APIPermission* rhs) const {
  CHECK(rhs->info() == info());
  const BluetoothDevicePermission* perm =
      static_cast<const BluetoothDevicePermission*>(rhs);
  return devices_ == perm->devices_;
}

bool BluetoothDevicePermission::FromValue(const base::Value* value) {
  devices_.clear();
  const base::ListValue* list = NULL;

  if (!value)
    return false;

  if (!value->GetAsList(&list) || list->GetSize() == 0)
    return false;

  for (size_t i = 0; i < list->GetSize(); ++i) {
    std::string device_address;
    if (!list->GetString(i, &device_address))
      return false;
    devices_.insert(device_address);
  }

  return true;
}

void BluetoothDevicePermission::ToValue(base::Value** value) const {
  base::ListValue* list = new ListValue();
  std::set<std::string>::const_iterator i;
  for (std::set<std::string>::const_iterator i = devices_.begin();
      i != devices_.end(); ++i) {
    list->Append(base::Value::CreateStringValue(*i));
  }
  *value = list;
}

APIPermission* BluetoothDevicePermission::Clone() const {
  BluetoothDevicePermission* result = new BluetoothDevicePermission(info());
  result->devices_ = devices_;
  return result;
}

APIPermission* BluetoothDevicePermission::Diff(const APIPermission* rhs) const {
  CHECK(rhs->info() == info());
  const BluetoothDevicePermission* perm =
      static_cast<const BluetoothDevicePermission*>(rhs);
  scoped_ptr<BluetoothDevicePermission> result(
      new BluetoothDevicePermission(info()));
  std::set_difference(
      devices_.begin(), devices_.end(),
      perm->devices_.begin(), perm->devices_.end(),
      std::inserter<std::set<std::string> >(
          result->devices_, result->devices_.begin()));
  return result->devices_.empty() ? NULL : result.release();
}

APIPermission* BluetoothDevicePermission::Union(
    const APIPermission* rhs) const {
  CHECK(rhs->info() == info());
  const BluetoothDevicePermission* perm =
      static_cast<const BluetoothDevicePermission*>(rhs);
  scoped_ptr<BluetoothDevicePermission> result(
      new BluetoothDevicePermission(info()));
  std::set_union(
      devices_.begin(), devices_.end(),
      perm->devices_.begin(), perm->devices_.end(),
      std::inserter<std::set<std::string> >(
          result->devices_, result->devices_.begin()));
  return result->devices_.empty() ? NULL : result.release();
}

APIPermission* BluetoothDevicePermission::Intersect(
    const APIPermission* rhs) const {
  CHECK(rhs->info() == info());
  const BluetoothDevicePermission* perm =
      static_cast<const BluetoothDevicePermission*>(rhs);
  scoped_ptr<BluetoothDevicePermission> result(
      new BluetoothDevicePermission(info()));
  std::set_intersection(
      devices_.begin(), devices_.end(),
      perm->devices_.begin(), perm->devices_.end(),
      std::inserter<std::set<std::string> >(
          result->devices_, result->devices_.begin()));
  return result->devices_.empty() ? NULL : result.release();
}

void BluetoothDevicePermission::Write(IPC::Message* m) const {
  IPC::WriteParam(m, devices_);
}

bool BluetoothDevicePermission::Read(
    const IPC::Message* m, PickleIterator* iter) {
  return IPC::ReadParam(m, iter, &devices_);
}

void BluetoothDevicePermission::Log(std::string* log) const {
  IPC::LogParam(devices_, log);
}

}  // namespace extensions
