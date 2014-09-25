// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/extensions_api_permissions.h"

#include <vector>

#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permission_message.h"
#include "extensions/common/permissions/socket_permission.h"
#include "extensions/common/permissions/usb_device_permission.h"
#include "grit/extensions_strings.h"

namespace extensions {

namespace {

const char kOldAlwaysOnTopWindowsPermission[] = "alwaysOnTopWindows";
const char kOldFullscreenPermission[] = "fullscreen";
const char kOldOverrideEscFullscreenPermission[] = "overrideEscFullscreen";

template <typename T>
APIPermission* CreateAPIPermission(const APIPermissionInfo* permission) {
  return new T(permission);
}

}  // namespace

std::vector<APIPermissionInfo*> ExtensionsAPIPermissions::GetAllPermissions()
    const {
  APIPermissionInfo::InitInfo permissions_to_register[] = {
      {APIPermission::kAlphaEnabled, "app.window.alpha"},
      {APIPermission::kAlwaysOnTopWindows, "app.window.alwaysOnTop"},
      {APIPermission::kAudioCapture, "audioCapture",
       APIPermissionInfo::kFlagNone, IDS_EXTENSION_PROMPT_WARNING_AUDIO_CAPTURE,
       PermissionMessage::kAudioCapture},
      {APIPermission::kDns, "dns"},
      {APIPermission::kExternallyConnectableAllUrls,
       "externally_connectable.all_urls"},
      {APIPermission::kFullscreen, "app.window.fullscreen"},
      {APIPermission::kHid, "hid", APIPermissionInfo::kFlagNone,
       IDS_EXTENSION_PROMPT_WARNING_HID, PermissionMessage::kHid},
      {APIPermission::kOverrideEscFullscreen,
       "app.window.fullscreen.overrideEsc"},
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
      {APIPermission::kSystemCpu, "system.cpu"},
      {APIPermission::kSystemMemory, "system.memory"},
      {APIPermission::kSystemNetwork, "system.network"},
      {APIPermission::kSystemDisplay, "system.display"},
      {APIPermission::kSystemStorage, "system.storage"},
      {APIPermission::kU2fDevices, "u2fDevices", APIPermissionInfo::kFlagNone,
       IDS_EXTENSION_PROMPT_WARNING_U2F_DEVICES,
       PermissionMessage::kU2fDevices},
      {APIPermission::kUsb, "usb", APIPermissionInfo::kFlagNone,
       IDS_EXTENSION_PROMPT_WARNING_USB, PermissionMessage::kUsb},
      {APIPermission::kUsbDevice, "usbDevices", APIPermissionInfo::kFlagNone, 0,
       PermissionMessage::kNone, &CreateAPIPermission<UsbDevicePermission>},
      {APIPermission::kVideoCapture, "videoCapture",
       APIPermissionInfo::kFlagNone, IDS_EXTENSION_PROMPT_WARNING_VIDEO_CAPTURE,
       PermissionMessage::kVideoCapture},
      // NOTE(kalman): This is provided by a manifest property but needs to
      // appear in the install permission dialogue, so we need a fake
      // permission for it. See http://crbug.com/247857.
      {APIPermission::kWebConnectable, "webConnectable",
       APIPermissionInfo::kFlagCannotBeOptional |
           APIPermissionInfo::kFlagInternal,
       IDS_EXTENSION_PROMPT_WARNING_WEB_CONNECTABLE,
       PermissionMessage::kWebConnectable},
      {APIPermission::kWebView, "webview",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kWindowShape, "app.window.shape"},
  };

  std::vector<APIPermissionInfo*> permissions;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(permissions_to_register); ++i)
    permissions.push_back(new APIPermissionInfo(permissions_to_register[i]));
  return permissions;
}

std::vector<PermissionsProvider::AliasInfo>
ExtensionsAPIPermissions::GetAllAliases() const {
  std::vector<PermissionsProvider::AliasInfo> aliases;
  aliases.push_back(PermissionsProvider::AliasInfo(
      "app.window.alwaysOnTop", kOldAlwaysOnTopWindowsPermission));
  aliases.push_back(PermissionsProvider::AliasInfo("app.window.fullscreen",
                                                   kOldFullscreenPermission));
  aliases.push_back(
      PermissionsProvider::AliasInfo("app.window.fullscreen.overrideEsc",
                                     kOldOverrideEscFullscreenPermission));
  return aliases;
}

}  // namespace extensions
