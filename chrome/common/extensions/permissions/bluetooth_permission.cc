// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/bluetooth_permission.h"

#include <string>

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/extensions/permissions/bluetooth_permission_data.h"
#include "chrome/common/extensions/permissions/permissions_info.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

BluetoothPermission::BluetoothPermission(const APIPermissionInfo* info)
  : SetDisjunctionPermission<BluetoothPermissionData,
                             BluetoothPermission>(info) {
}

BluetoothPermission::~BluetoothPermission() {
}

bool BluetoothPermission::FromValue(const base::Value* value) {
  // Value may be omitted to gain access to non-profile functions.
  if (!value)
    return true;

  // Value may be an empty list for the same reason.
  const base::ListValue* list = NULL;
  if (value->GetAsList(&list) && list->GetSize() == 0)
    return true;

  if (!SetDisjunctionPermission<BluetoothPermissionData,
                                BluetoothPermission>::FromValue(value)) {
    return false;
  }

  return true;
}

PermissionMessages BluetoothPermission::GetMessages() const {
  DCHECK(HasMessages());
  PermissionMessages result;

  result.push_back(PermissionMessage(
        PermissionMessage::kBluetooth,
        l10n_util::GetStringUTF16(
            IDS_EXTENSION_PROMPT_WARNING_BLUETOOTH)));

  if (!data_set_.empty()) {
    result.push_back(PermissionMessage(
          PermissionMessage::kBluetoothDevices,
          l10n_util::GetStringUTF16(
              IDS_EXTENSION_PROMPT_WARNING_BLUETOOTH_DEVICES)));
  }

  return result;
}

}  // namespace extensions
