// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/bluetooth_device_permission.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/extensions/permissions/bluetooth_device_permission_data.h"
#include "chrome/common/extensions/permissions/permissions_info.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

BluetoothDevicePermission::BluetoothDevicePermission(
    const APIPermissionInfo* info)
  : SetDisjunctionPermission<BluetoothDevicePermissionData,
                             BluetoothDevicePermission>(info) {
}

BluetoothDevicePermission::~BluetoothDevicePermission() {
}

bool BluetoothDevicePermission::ManifestEntryForbidden() const {
  return true;
}

PermissionMessages BluetoothDevicePermission::GetMessages() const {
  DCHECK(HasMessages());
  PermissionMessages result;

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter =
      device::BluetoothAdapterFactory::GetAdapter();

  for (std::set<BluetoothDevicePermissionData>::const_iterator i =
      data_set_.begin(); i != data_set_.end(); ++i) {

    const std::string& device_address = i->device_address();
    string16 device_identifier;
    if (bluetooth_adapter) {
      device::BluetoothDevice* device =
          bluetooth_adapter->GetDevice(device_address);
      if (device)
        device_identifier = device->GetName();
    }

    if (device_identifier.length() == 0) {
      UTF8ToUTF16(device_address.c_str(), device_address.length(),
          &device_identifier);
    }

    result.push_back(PermissionMessage(
          PermissionMessage::kBluetoothDevice,
          l10n_util::GetStringFUTF16(
              IDS_EXTENSION_PROMPT_WARNING_BLUETOOTH_DEVICE,
              device_identifier)));
  }

  return result;
}

}  // namespace extensions
