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
const char kOldUnlimitedStoragePermission[] = "unlimited_storage";

template <typename T>
APIPermission* CreateAPIPermission(const APIPermissionInfo* permission) {
  return new T(permission);
}

}  // namespace

std::vector<APIPermissionInfo*> ExtensionsAPIPermissions::GetAllPermissions()
    const {
  // WARNING: If you are modifying a permission message in this list, be sure to
  // add the corresponding permission message rule to
  // ChromePermissionMessageProvider::GetCoalescedPermissionMessages as well.
  // TODO(sashab): Remove all permission messages from this list, once
  // GetCoalescedPermissionMessages() is the only way of generating permission
  // messages.
  APIPermissionInfo::InitInfo permissions_to_register[] = {
      {APIPermission::kAlarms, "alarms"},
      {APIPermission::kAlphaEnabled, "app.window.alpha"},
      {APIPermission::kAlwaysOnTopWindows, "app.window.alwaysOnTop"},
      {APIPermission::kAppView,
       "appview",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kAudio, "audio"},
      {APIPermission::kAudioCapture,
       "audioCapture",
       APIPermissionInfo::kFlagNone,
       IDS_EXTENSION_PROMPT_WARNING_AUDIO_CAPTURE,
       PermissionMessage::kAudioCapture},
      {APIPermission::kBluetoothPrivate,
       "bluetoothPrivate",
       APIPermissionInfo::kFlagCannotBeOptional,
       IDS_EXTENSION_PROMPT_WARNING_BLUETOOTH_PRIVATE,
       PermissionMessage::kBluetoothPrivate},
      {APIPermission::kClipboardRead,
       "clipboardRead",
       APIPermissionInfo::kFlagSupportsContentCapabilities,
       IDS_EXTENSION_PROMPT_WARNING_CLIPBOARD,
       PermissionMessage::kClipboard},
      {APIPermission::kClipboardWrite,
       "clipboardWrite",
       APIPermissionInfo::kFlagSupportsContentCapabilities},
      {APIPermission::kDeclarativeWebRequest,
       "declarativeWebRequest",
       APIPermissionInfo::kFlagNone,
       IDS_EXTENSION_PROMPT_WARNING_DECLARATIVE_WEB_REQUEST,
       PermissionMessage::kDeclarativeWebRequest},
      {APIPermission::kDiagnostics,
       "diagnostics",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kDns, "dns"},
      {APIPermission::kDocumentScan,
       "documentScan",
       APIPermissionInfo::kFlagNone,
       IDS_EXTENSION_PROMPT_WARNING_DOCUMENT_SCAN,
       PermissionMessage::kDocumentScan},
      {APIPermission::kExtensionView,
       "extensionview",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kExternallyConnectableAllUrls,
       "externally_connectable.all_urls"},
      {APIPermission::kFullscreen, "app.window.fullscreen"},
      {APIPermission::kHid, "hid", APIPermissionInfo::kFlagNone},
      {APIPermission::kImeWindowEnabled, "app.window.ime"},
      {APIPermission::kOverrideEscFullscreen,
       "app.window.fullscreen.overrideEsc"},
      {APIPermission::kIdle, "idle"},
      {APIPermission::kNetworkingConfig,
       "networking.config",
       APIPermissionInfo::kFlagNone,
       IDS_EXTENSION_PROMPT_WARNING_NETWORKING_CONFIG,
       PermissionMessage::kNetworkingConfig},
      {APIPermission::kNetworkingPrivate,
       "networkingPrivate",
       APIPermissionInfo::kFlagCannotBeOptional,
       IDS_EXTENSION_PROMPT_WARNING_NETWORKING_PRIVATE,
       PermissionMessage::kNetworkingPrivate},
      {APIPermission::kPower, "power"},
      {APIPermission::kPrinterProvider, "printerProvider"},
      {APIPermission::kSerial,
       "serial",
       APIPermissionInfo::kFlagNone,
       IDS_EXTENSION_PROMPT_WARNING_SERIAL,
       PermissionMessage::kSerial},
      // Because warning messages for the "socket" permission vary based
      // on the permissions parameters, no message ID or message text is
      // specified here.  The message ID and text used will be
      // determined at run-time in the |SocketPermission| class.
      {APIPermission::kSocket,
       "socket",
       APIPermissionInfo::kFlagCannotBeOptional,
       0,
       PermissionMessage::kNone,
       &CreateAPIPermission<SocketPermission>},
      {APIPermission::kStorage, "storage"},
      {APIPermission::kSystemCpu, "system.cpu"},
      {APIPermission::kSystemMemory, "system.memory"},
      {APIPermission::kSystemNetwork, "system.network"},
      {APIPermission::kSystemDisplay, "system.display"},
      {APIPermission::kSystemStorage, "system.storage"},
      {APIPermission::kU2fDevices,
       "u2fDevices",
       APIPermissionInfo::kFlagNone,
       IDS_EXTENSION_PROMPT_WARNING_U2F_DEVICES,
       PermissionMessage::kU2fDevices},
      {APIPermission::kUnlimitedStorage,
       "unlimitedStorage",
       APIPermissionInfo::kFlagCannotBeOptional |
           APIPermissionInfo::kFlagSupportsContentCapabilities},
      {APIPermission::kUsb, "usb", APIPermissionInfo::kFlagNone},
      {APIPermission::kUsbDevice,
       "usbDevices",
       APIPermissionInfo::kFlagNone,
       0,
       PermissionMessage::kNone,
       &CreateAPIPermission<UsbDevicePermission>},
      {APIPermission::kVideoCapture,
       "videoCapture",
       APIPermissionInfo::kFlagNone,
       IDS_EXTENSION_PROMPT_WARNING_VIDEO_CAPTURE,
       PermissionMessage::kVideoCapture},
      {APIPermission::kVpnProvider,
       "vpnProvider",
       APIPermissionInfo::kFlagCannotBeOptional,
       IDS_EXTENSION_PROMPT_WARNING_VPN,
       PermissionMessage::kVpnProvider},
      // NOTE(kalman): This is provided by a manifest property but needs to
      // appear in the install permission dialogue, so we need a fake
      // permission for it. See http://crbug.com/247857.
      {APIPermission::kWebConnectable,
       "webConnectable",
       APIPermissionInfo::kFlagCannotBeOptional |
           APIPermissionInfo::kFlagInternal,
       IDS_EXTENSION_PROMPT_WARNING_WEB_CONNECTABLE,
       PermissionMessage::kWebConnectable},
      {APIPermission::kWebRequest, "webRequest"},
      {APIPermission::kWebRequestBlocking, "webRequestBlocking"},
      {APIPermission::kWebView,
       "webview",
       APIPermissionInfo::kFlagCannotBeOptional},
      {APIPermission::kWindowShape, "app.window.shape"},
  };

  std::vector<APIPermissionInfo*> permissions;
  for (size_t i = 0; i < arraysize(permissions_to_register); ++i)
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
  aliases.push_back(PermissionsProvider::AliasInfo(
      "unlimitedStorage", kOldUnlimitedStoragePermission));
  return aliases;
}

}  // namespace extensions
