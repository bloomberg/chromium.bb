// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/bluetooth/bluetooth_manifest_data.h"

#include <utility>

#include "extensions/common/api/bluetooth/bluetooth_manifest_permission.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

BluetoothManifestData::BluetoothManifestData(
    std::unique_ptr<BluetoothManifestPermission> permission)
    : permission_(std::move(permission)) {
  DCHECK(permission_);
}

BluetoothManifestData::~BluetoothManifestData() {}

// static
BluetoothManifestData* BluetoothManifestData::Get(const Extension* extension) {
  return static_cast<BluetoothManifestData*>(
      extension->GetManifestData(manifest_keys::kBluetooth));
}

// static
bool BluetoothManifestData::CheckRequest(
    const Extension* extension,
    const BluetoothPermissionRequest& request) {
  const BluetoothManifestData* data = BluetoothManifestData::Get(extension);
  return data && data->permission()->CheckRequest(extension, request);
}

// static
bool BluetoothManifestData::CheckSocketPermitted(
    const Extension* extension) {
  const BluetoothManifestData* data = BluetoothManifestData::Get(extension);
  return data && data->permission()->CheckSocketPermitted(extension);
}

// static
bool BluetoothManifestData::CheckLowEnergyPermitted(
    const Extension* extension) {
  const BluetoothManifestData* data = BluetoothManifestData::Get(extension);
  return data && data->permission()->CheckLowEnergyPermitted(extension);
}

// static
bool BluetoothManifestData::CheckPeripheralPermitted(
    const Extension* extension) {
  const BluetoothManifestData* data = BluetoothManifestData::Get(extension);
  return data && data->permission()->CheckLowEnergyPermitted(extension) &&
         data->permission()->CheckPeripheralPermitted(extension);
}

// static
std::unique_ptr<BluetoothManifestData> BluetoothManifestData::FromValue(
    const base::Value& value,
    base::string16* error) {
  std::unique_ptr<BluetoothManifestPermission> permission =
      BluetoothManifestPermission::FromValue(value, error);
  if (!permission)
    return std::unique_ptr<BluetoothManifestData>();

  return std::unique_ptr<BluetoothManifestData>(
      new BluetoothManifestData(std::move(permission)));
}

BluetoothPermissionRequest::BluetoothPermissionRequest(
    const std::string& uuid)
    : uuid(uuid) {}

BluetoothPermissionRequest::~BluetoothPermissionRequest() {}

}  // namespace extensions
