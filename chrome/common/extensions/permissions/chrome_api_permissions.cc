// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/chrome_api_permissions.h"

#include "chrome/common/extensions/permissions/api_permission.h"
#include "chrome/common/extensions/permissions/api_permission_set.h"
#include "chrome/common/extensions/permissions/bluetooth_device_permission.h"
#include "chrome/common/extensions/permissions/media_galleries_permission.h"
#include "chrome/common/extensions/permissions/permission_message.h"
#include "chrome/common/extensions/permissions/permissions_info.h"
#include "chrome/common/extensions/permissions/socket_permission.h"
#include "chrome/common/extensions/permissions/usb_device_permission.h"
#include "grit/generated_resources.h"

namespace extensions {

namespace {

const char kOldUnlimitedStoragePermission[] = "unlimited_storage";
const char kWindowsPermission[] = "windows";

template<typename T> APIPermission* CreateAPIPermission(
    const APIPermissionInfo* permission) {
  return new T(permission);
}

}  // namespace

std::vector<APIPermissionInfo*> ChromeAPIPermissions::GetAllPermissions()
    const {
  struct PermissionRegistration {
    APIPermission::ID id;
    const char* name;
    int flags;
    int l10n_message_id;
    PermissionMessage::ID message_id;
    APIPermissionInfo::APIPermissionConstructor constructor;
  } PermissionsToRegister[] = {
    // Register permissions for all extension types.
    { APIPermission::kBackground, "background" },
    { APIPermission::kClipboardRead, "clipboardRead",
      APIPermissionInfo::kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_CLIPBOARD,
      PermissionMessage::kClipboard },
    { APIPermission::kClipboardWrite, "clipboardWrite" },
    { APIPermission::kDeclarativeContent, "declarativeContent" },
    { APIPermission::kDeclarativeWebRequest, "declarativeWebRequest" },
    { APIPermission::kDownloads, "downloads", APIPermissionInfo::kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_DOWNLOADS,
      PermissionMessage::kDownloads },
    { APIPermission::kExperimental, "experimental",
      APIPermissionInfo::kFlagCannotBeOptional },
    { APIPermission::kGeolocation, "geolocation",
      APIPermissionInfo::kFlagCannotBeOptional,
      IDS_EXTENSION_PROMPT_WARNING_GEOLOCATION,
      PermissionMessage::kGeolocation },
    { APIPermission::kNotification, "notifications" },
    { APIPermission::kScreensaver, "screensaver" },
    { APIPermission::kUnlimitedStorage, "unlimitedStorage",
      APIPermissionInfo::kFlagCannotBeOptional },

    // Register extension permissions.
    { APIPermission::kActiveTab, "activeTab" },
    { APIPermission::kAdView, "adview" },
    { APIPermission::kAlarms, "alarms" },
    { APIPermission::kBookmark, "bookmarks", APIPermissionInfo::kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_BOOKMARKS,
      PermissionMessage::kBookmarks },
    { APIPermission::kBrowsingData, "browsingData" },
    { APIPermission::kContentSettings, "contentSettings",
      APIPermissionInfo::kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_CONTENT_SETTINGS,
      PermissionMessage::kContentSettings },
    { APIPermission::kContextMenus, "contextMenus" },
    { APIPermission::kCookie, "cookies" },
    { APIPermission::kFileBrowserHandler, "fileBrowserHandler",
      APIPermissionInfo::kFlagCannotBeOptional },
    { APIPermission::kFontSettings, "fontSettings",
      APIPermissionInfo::kFlagCannotBeOptional },
    { APIPermission::kHistory, "history", APIPermissionInfo::kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_BROWSING_HISTORY,
      PermissionMessage::kBrowsingHistory },
    { APIPermission::kIdle, "idle" },
    { APIPermission::kInput, "input", APIPermissionInfo::kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_INPUT,
      PermissionMessage::kInput },
    { APIPermission::kManagement, "management", APIPermissionInfo::kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_MANAGEMENT,
      PermissionMessage::kManagement },
    { APIPermission::kNativeMessaging, "nativeMessaging" },
    { APIPermission::kPower, "power", },
    { APIPermission::kPrivacy, "privacy", APIPermissionInfo::kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_PRIVACY,
      PermissionMessage::kPrivacy },
    { APIPermission::kSessionRestore, "sessionRestore" },
    { APIPermission::kStorage, "storage" },
    { APIPermission::kSyncFileSystem, "syncFileSystem",
      APIPermissionInfo::kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_SYNCFILESYSTEM,
      PermissionMessage::kSyncFileSystem },
    { APIPermission::kTab, "tabs", APIPermissionInfo::kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_TABS,
      PermissionMessage::kTabs },
    { APIPermission::kTopSites, "topSites", APIPermissionInfo::kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_BROWSING_HISTORY,
      PermissionMessage::kBrowsingHistory },
    { APIPermission::kTts, "tts", 0, APIPermissionInfo::kFlagCannotBeOptional },
    { APIPermission::kTtsEngine, "ttsEngine",
      APIPermissionInfo::kFlagCannotBeOptional,
      IDS_EXTENSION_PROMPT_WARNING_TTS_ENGINE,
      PermissionMessage::kTtsEngine },
    { APIPermission::kWebNavigation, "webNavigation",
      APIPermissionInfo::kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_TABS, PermissionMessage::kTabs },
    { APIPermission::kWebRequest, "webRequest" },
    { APIPermission::kWebRequestBlocking, "webRequestBlocking" },
    { APIPermission::kWebView, "webview",
      APIPermissionInfo::kFlagCannotBeOptional },

    // Register private permissions.
    { APIPermission::kAutoTestPrivate, "autotestPrivate",
      APIPermissionInfo::kFlagCannotBeOptional },
    { APIPermission::kBookmarkManagerPrivate, "bookmarkManagerPrivate",
      APIPermissionInfo::kFlagCannotBeOptional },
    { APIPermission::kChromeosInfoPrivate, "chromeosInfoPrivate",
      APIPermissionInfo::kFlagCannotBeOptional },
    { APIPermission::kDeveloperPrivate, "developerPrivate",
      APIPermissionInfo::kFlagCannotBeOptional },
    { APIPermission::kDial, "dial", APIPermissionInfo::kFlagCannotBeOptional },
    { APIPermission::kDownloadsInternal, "downloadsInternal" },
    { APIPermission::kFileBrowserHandlerInternal, "fileBrowserHandlerInternal",
      APIPermissionInfo::kFlagCannotBeOptional },
    { APIPermission::kFileBrowserPrivate, "fileBrowserPrivate",
      APIPermissionInfo::kFlagCannotBeOptional },
    { APIPermission::kNetworkingPrivate, "networkingPrivate",
      APIPermissionInfo::kFlagCannotBeOptional },
    { APIPermission::kManagedModePrivate, "managedModePrivate",
      APIPermissionInfo::kFlagCannotBeOptional },
    { APIPermission::kMediaPlayerPrivate, "mediaPlayerPrivate",
      APIPermissionInfo::kFlagCannotBeOptional },
    { APIPermission::kMetricsPrivate, "metricsPrivate",
      APIPermissionInfo::kFlagCannotBeOptional },
    { APIPermission::kSystemPrivate, "systemPrivate",
      APIPermissionInfo::kFlagCannotBeOptional },
    { APIPermission::kCloudPrintPrivate, "cloudPrintPrivate",
      APIPermissionInfo::kFlagCannotBeOptional },
    { APIPermission::kInputMethodPrivate, "inputMethodPrivate",
      APIPermissionInfo::kFlagCannotBeOptional },
    { APIPermission::kEchoPrivate, "echoPrivate",
      APIPermissionInfo::kFlagCannotBeOptional },
    { APIPermission::kRtcPrivate, "rtcPrivate",
      APIPermissionInfo::kFlagCannotBeOptional },
    { APIPermission::kTerminalPrivate, "terminalPrivate",
      APIPermissionInfo::kFlagCannotBeOptional },
    { APIPermission::kWallpaperPrivate, "wallpaperPrivate",
      APIPermissionInfo::kFlagCannotBeOptional },
    { APIPermission::kWebRequestInternal, "webRequestInternal" },
    { APIPermission::kWebSocketProxyPrivate, "webSocketProxyPrivate",
      APIPermissionInfo::kFlagCannotBeOptional },
    { APIPermission::kWebstorePrivate, "webstorePrivate",
      APIPermissionInfo::kFlagCannotBeOptional },
    { APIPermission::kMediaGalleriesPrivate, "mediaGalleriesPrivate",
      APIPermissionInfo::kFlagCannotBeOptional },
    { APIPermission::kStreamsPrivate, "streamsPrivate",
      APIPermissionInfo::kFlagCannotBeOptional },

    // Full url access permissions.
    { APIPermission::kDebugger, "debugger",
      APIPermissionInfo::kFlagImpliesFullURLAccess |
          APIPermissionInfo::kFlagCannotBeOptional,
      IDS_EXTENSION_PROMPT_WARNING_DEBUGGER,
      PermissionMessage::kDebugger },
    { APIPermission::kDevtools, "devtools",
      APIPermissionInfo::kFlagImpliesFullURLAccess |
      APIPermissionInfo::kFlagCannotBeOptional },
    { APIPermission::kPageCapture, "pageCapture",
      APIPermissionInfo::kFlagImpliesFullURLAccess },
    { APIPermission::kTabCapture, "tabCapture",
      APIPermissionInfo::kFlagImpliesFullURLAccess },
    { APIPermission::kPlugin, "plugin",
      APIPermissionInfo::kFlagImpliesFullURLAccess |
          APIPermissionInfo::kFlagImpliesFullAccess |
          APIPermissionInfo::kFlagCannotBeOptional,
      IDS_EXTENSION_PROMPT_WARNING_FULL_ACCESS,
      PermissionMessage::kFullAccess },
    { APIPermission::kProxy, "proxy",
      APIPermissionInfo::kFlagImpliesFullURLAccess |
          APIPermissionInfo::kFlagCannotBeOptional },

    // Platform-app permissions.
    { APIPermission::kSerial, "serial", APIPermissionInfo::kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_SERIAL,
      PermissionMessage::kSerial },
    // Because warning messages for the "socket" permission vary based on the
    // permissions parameters, no message ID or message text is specified here.
    // The message ID and text used will be determined at run-time in the
    // |SocketPermission| class.
    { APIPermission::kSocket, "socket",
      APIPermissionInfo::kFlagCannotBeOptional, 0,
      PermissionMessage::kNone, &CreateAPIPermission<SocketPermission> },
    { APIPermission::kAppCurrentWindowInternal, "app.currentWindowInternal" },
    { APIPermission::kAppRuntime, "app.runtime" },
    { APIPermission::kAppWindow, "app.window" },
    { APIPermission::kAudioCapture, "audioCapture",
      APIPermissionInfo::kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_AUDIO_CAPTURE,
      PermissionMessage::kAudioCapture },
    { APIPermission::kVideoCapture, "videoCapture",
      APIPermissionInfo::kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_VIDEO_CAPTURE,
      PermissionMessage::kVideoCapture },
    // The permission string for "fileSystem" is only shown when "write" is
    // present. Read-only access is only granted after the user has been shown
    // a file chooser dialog and selected a file. Selecting the file is
    // considered consent to read it.
    { APIPermission::kFileSystem, "fileSystem" },
    { APIPermission::kFileSystemWrite, "fileSystem.write",
      APIPermissionInfo::kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_FILE_SYSTEM_WRITE,
      PermissionMessage::kFileSystemWrite },
    // Because warning messages for the "mediaGalleries" permission vary based
    // on the permissions parameters, no message ID or message text is
    // specified here.
    // The message ID and text used will be determined at run-time in the
    // |MediaGalleriesPermission| class.
    { APIPermission::kMediaGalleries, "mediaGalleries",
      APIPermissionInfo::kFlagNone, 0,
      PermissionMessage::kNone,
      &CreateAPIPermission<MediaGalleriesPermission> },
    { APIPermission::kPushMessaging, "pushMessaging",
      APIPermissionInfo::kFlagCannotBeOptional },
    { APIPermission::kBluetooth, "bluetooth", APIPermissionInfo::kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_BLUETOOTH,
      PermissionMessage::kBluetooth },
    { APIPermission::kBluetoothDevice, "bluetoothDevices",
      APIPermissionInfo::kFlagNone, 0, PermissionMessage::kNone,
      &CreateAPIPermission<BluetoothDevicePermission> },
    { APIPermission::kUsb, "usb", APIPermissionInfo::kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_USB,
      PermissionMessage::kUsb },
    { APIPermission::kUsbDevice, "usbDevices",
      APIPermissionInfo::kFlagMustBeOptional, 0, PermissionMessage::kNone,
      &CreateAPIPermission<UsbDevicePermission> },
    { APIPermission::kSystemIndicator, "systemIndicator",
      APIPermissionInfo::kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_SYSTEM_INDICATOR,
      PermissionMessage::kSystemIndicator },
    { APIPermission::kSystemInfoDisplay, "systemInfo.display" },
    { APIPermission::kPointerLock, "pointerLock" },
    { APIPermission::kFullscreen, "fullscreen" },
  };

  std::vector<APIPermissionInfo*> permissions;

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(PermissionsToRegister); ++i) {
    const PermissionRegistration& pr = PermissionsToRegister[i];
    permissions.push_back(new APIPermissionInfo(
        pr.id, pr.name, pr.l10n_message_id,
        pr.message_id ? pr.message_id : PermissionMessage::kNone,
        pr.flags,
        pr.constructor));
  }
  return permissions;
}

std::vector<PermissionsInfo::AliasInfo> ChromeAPIPermissions::GetAllAliases()
    const {
  // Register aliases.
  std::vector<PermissionsInfo::AliasInfo> aliases;
  aliases.push_back(PermissionsInfo::AliasInfo(
      "unlimitedStorage", kOldUnlimitedStoragePermission));
  aliases.push_back(PermissionsInfo::AliasInfo(
      "tabs", kWindowsPermission));
  return aliases;
}

}  // namespace extensions
