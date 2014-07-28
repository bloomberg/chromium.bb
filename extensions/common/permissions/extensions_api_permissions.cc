// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/extensions_api_permissions.h"

#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permission_message.h"
#include "extensions/common/permissions/socket_permission.h"
#include "extensions/common/permissions/usb_device_permission.h"
#include "grit/extensions_strings.h"

namespace extensions {

namespace {

template <typename T>
APIPermission* CreateAPIPermission(const APIPermissionInfo* permission) {
  return new T(permission);
}

}  // namespace

std::vector<APIPermissionInfo*> ExtensionsAPIPermissions::GetAllPermissions()
    const {
  APIPermissionInfo::InitInfo permissions_to_register[] = {
      {APIPermission::kDns, "dns"},
      {APIPermission::kHid, "hid", APIPermissionInfo::kFlagNone,
       IDS_EXTENSION_PROMPT_WARNING_HID, PermissionMessage::kHid},
      {APIPermission::kPower, "power"},
      {APIPermission::kSerial, "serial", APIPermissionInfo::kFlagNone,
       IDS_EXTENSION_PROMPT_WARNING_SERIAL, PermissionMessage::kSerial},
      // Because warning messages for the "socket" permission vary based
      // on the permissions parameters, no message ID or message text is
      // specified here.  The message ID and text used will be
      // determined at run-time in the |SocketPermission| class.
      {APIPermission::kSocket, "socket",
       APIPermissionInfo::kFlagCannotBeOptional, 0, PermissionMessage::kNone,
       &CreateAPIPermission<SocketPermission>},
      {APIPermission::kStorage, "storage"},
      {APIPermission::kUsb, "usb", APIPermissionInfo::kFlagNone,
       IDS_EXTENSION_PROMPT_WARNING_USB, PermissionMessage::kUsb},
      {APIPermission::kUsbDevice, "usbDevices", APIPermissionInfo::kFlagNone, 0,
       PermissionMessage::kNone, &CreateAPIPermission<UsbDevicePermission>},
  };

  std::vector<APIPermissionInfo*> permissions;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(permissions_to_register); ++i)
    permissions.push_back(new APIPermissionInfo(permissions_to_register[i]));
  return permissions;
}

std::vector<PermissionsProvider::AliasInfo>
ExtensionsAPIPermissions::GetAllAliases() const {
  return std::vector<PermissionsProvider::AliasInfo>();
}

}  // namespace extensions
