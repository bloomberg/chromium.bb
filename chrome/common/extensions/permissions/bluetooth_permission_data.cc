// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/bluetooth_permission_data.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/extensions/permissions/bluetooth_permission.h"
#include "device/bluetooth/bluetooth_utils.h"

namespace {

const char* kUuidKey = "uuid";

}  // namespace

namespace extensions {

BluetoothPermissionData::BluetoothPermissionData() {}

BluetoothPermissionData::BluetoothPermissionData(
    const std::string& uuid) : uuid_(uuid) {
}

bool BluetoothPermissionData::Check(
    const APIPermission::CheckParam* param) const {
  if (!param)
    return false;
  const BluetoothPermission::CheckParam& specific_param =
      *static_cast<const BluetoothPermission::CheckParam*>(param);

  std::string canonical_uuid = device::bluetooth_utils::CanonicalUuid(uuid_);
  std::string canonical_param_uuid = device::bluetooth_utils::CanonicalUuid(
      specific_param.uuid);
  return canonical_uuid == canonical_param_uuid;
}

scoped_ptr<base::Value> BluetoothPermissionData::ToValue() const {
  base::DictionaryValue* result = new base::DictionaryValue();
  result->SetString(kUuidKey, uuid_);
  return scoped_ptr<base::Value>(result);
}

bool BluetoothPermissionData::FromValue(const base::Value* value) {
  if (!value)
    return false;

  const base::DictionaryValue* dict_value;
  if (!value->GetAsDictionary(&dict_value))
    return false;

  if (!dict_value->GetString(kUuidKey, &uuid_)) {
    uuid_.clear();
    return false;
  }

  return true;
}

bool BluetoothPermissionData::operator<(
    const BluetoothPermissionData& rhs) const {
  return uuid_ < rhs.uuid_;
}

bool BluetoothPermissionData::operator==(
    const BluetoothPermissionData& rhs) const {
  return uuid_ == rhs.uuid_;
}

}  // namespace extensions
