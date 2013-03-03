// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/api_permission.h"

#include "chrome/common/extensions/permissions/bluetooth_device_permission.h"
#include "chrome/common/extensions/permissions/media_galleries_permission.h"
#include "chrome/common/extensions/permissions/permissions_info.h"
#include "chrome/common/extensions/permissions/socket_permission.h"
#include "chrome/common/extensions/permissions/usb_device_permission.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using extensions::APIPermission;
using extensions::APIPermissionInfo;
using extensions::PermissionMessage;
using extensions::PermissionMessages;

const char kOldUnlimitedStoragePermission[] = "unlimited_storage";
const char kWindowsPermission[] = "windows";

class SimpleAPIPermission : public APIPermission {
 public:
  explicit SimpleAPIPermission(const APIPermissionInfo* permission)
    : APIPermission(permission) { }

  virtual ~SimpleAPIPermission() { }

  virtual bool HasMessages() const OVERRIDE {
    return info()->message_id() > PermissionMessage::kNone;
  }

  virtual PermissionMessages GetMessages() const OVERRIDE {
    DCHECK(HasMessages());
    PermissionMessages result;
    result.push_back(GetMessage_());
    return result;
  }

  virtual bool Check(
      const APIPermission::CheckParam* param) const OVERRIDE {
    return !param;
  }

  virtual bool Contains(const APIPermission* rhs) const OVERRIDE {
    CHECK(info() == rhs->info());
    return true;
  }

  virtual bool Equal(const APIPermission* rhs) const OVERRIDE {
    if (this == rhs)
      return true;
    CHECK(info() == rhs->info());
    return true;
  }

  virtual bool FromValue(const base::Value* value) OVERRIDE {
    if (value)
      return false;
    return true;
  }

  virtual scoped_ptr<base::Value> ToValue() const OVERRIDE {
    return scoped_ptr<base::Value>(NULL);
  }

  virtual APIPermission* Clone() const OVERRIDE {
    return new SimpleAPIPermission(info());
  }

  virtual APIPermission* Diff(const APIPermission* rhs) const OVERRIDE {
    CHECK(info() == rhs->info());
    return NULL;
  }

  virtual APIPermission* Union(const APIPermission* rhs) const OVERRIDE {
    CHECK(info() == rhs->info());
    return new SimpleAPIPermission(info());
  }

  virtual APIPermission* Intersect(const APIPermission* rhs) const OVERRIDE {
    CHECK(info() == rhs->info());
    return new SimpleAPIPermission(info());
  }

  virtual void Write(IPC::Message* m) const OVERRIDE { }

  virtual bool Read(const IPC::Message* m, PickleIterator* iter) OVERRIDE {
    return true;
  }

  virtual void Log(std::string* log) const OVERRIDE { }
};

template<typename T>
APIPermission* CreateAPIPermission(const APIPermissionInfo* permission) {
  return new T(permission);
}

}  // namespace

namespace extensions {

APIPermission::APIPermission(const APIPermissionInfo* info)
  : info_(info) {
  DCHECK(info_);
}

APIPermission::~APIPermission() { }

APIPermission::ID APIPermission::id() const {
  return info()->id();
}

const char* APIPermission::name() const {
  return info()->name();
}

bool APIPermission::ManifestEntryForbidden() const {
  return false;
}

PermissionMessage APIPermission::GetMessage_() const {
  return info()->GetMessage_();
}

//
// APIPermissionInfo
//

APIPermissionInfo::APIPermissionInfo(
    APIPermission::ID id,
    const char* name,
    int l10n_message_id,
    PermissionMessage::ID message_id,
    int flags,
    APIPermissionConstructor api_permission_constructor)
    : id_(id),
      name_(name),
      flags_(flags),
      l10n_message_id_(l10n_message_id),
      message_id_(message_id),
      api_permission_constructor_(api_permission_constructor) { }


APIPermissionInfo::~APIPermissionInfo() { }

APIPermission* APIPermissionInfo::CreateAPIPermission() const {
  return api_permission_constructor_ ?
    api_permission_constructor_(this) : new SimpleAPIPermission(this);
}

PermissionMessage APIPermissionInfo::GetMessage_() const {
  return PermissionMessage(
      message_id_, l10n_util::GetStringUTF16(l10n_message_id_));
}

// static
void APIPermissionInfo::RegisterAllPermissions(
    PermissionsInfo* info) {

  struct PermissionRegistration {
    APIPermission::ID id;
    const char* name;
    int flags;
    int l10n_message_id;
    PermissionMessage::ID message_id;
    APIPermissionConstructor constructor;
  } PermissionsToRegister[] = {
    // Register permissions for all extension types.
    { APIPermission::kBackground, "background" },
    { APIPermission::kClipboardRead, "clipboardRead", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_CLIPBOARD,
      PermissionMessage::kClipboard },
    { APIPermission::kClipboardWrite, "clipboardWrite" },
    { APIPermission::kDeclarativeContent, "declarativeContent" },
    { APIPermission::kDeclarativeWebRequest, "declarativeWebRequest" },
    { APIPermission::kDownloads, "downloads", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_DOWNLOADS,
      PermissionMessage::kDownloads },
    { APIPermission::kExperimental, "experimental", kFlagCannotBeOptional },
    { APIPermission::kGeolocation, "geolocation", kFlagCannotBeOptional,
      IDS_EXTENSION_PROMPT_WARNING_GEOLOCATION,
      PermissionMessage::kGeolocation },
    { APIPermission::kNotification, "notifications" },
    { APIPermission::kScreensaver, "screensaver" },
    { APIPermission::kUnlimitedStorage, "unlimitedStorage",
      kFlagCannotBeOptional },

    // Register hosted and packaged app permissions.
    { APIPermission::kAppNotifications, "appNotifications" },

    // Register extension permissions.
    { APIPermission::kActiveTab, "activeTab" },
    { APIPermission::kAlarms, "alarms" },
    { APIPermission::kBookmark, "bookmarks", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_BOOKMARKS,
      PermissionMessage::kBookmarks },
    { APIPermission::kBrowsingData, "browsingData" },
    { APIPermission::kContentSettings, "contentSettings", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_CONTENT_SETTINGS,
      PermissionMessage::kContentSettings },
    { APIPermission::kContextMenus, "contextMenus" },
    { APIPermission::kCookie, "cookies" },
    { APIPermission::kFileBrowserHandler, "fileBrowserHandler",
      kFlagCannotBeOptional },
    { APIPermission::kFontSettings, "fontSettings", kFlagCannotBeOptional },
    { APIPermission::kHistory, "history", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_BROWSING_HISTORY,
      PermissionMessage::kBrowsingHistory },
    { APIPermission::kIdle, "idle" },
    { APIPermission::kInput, "input", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_INPUT,
      PermissionMessage::kInput },
    { APIPermission::kManagement, "management", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_MANAGEMENT,
      PermissionMessage::kManagement },
    { APIPermission::kPrivacy, "privacy", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_PRIVACY,
      PermissionMessage::kPrivacy },
    { APIPermission::kSessionRestore, "sessionRestore" },
    { APIPermission::kStorage, "storage" },
    // TODO(kinuko): syncFileSystem permission should take the service name
    // parameter.
    { APIPermission::kSyncFileSystem, "syncFileSystem" },
    { APIPermission::kTab, "tabs", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_TABS,
      PermissionMessage::kTabs },
    { APIPermission::kTopSites, "topSites", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_BROWSING_HISTORY,
      PermissionMessage::kBrowsingHistory },
    { APIPermission::kTts, "tts", 0, kFlagCannotBeOptional },
    { APIPermission::kTtsEngine, "ttsEngine", kFlagCannotBeOptional,
      IDS_EXTENSION_PROMPT_WARNING_TTS_ENGINE,
      PermissionMessage::kTtsEngine },
    { APIPermission::kWebNavigation, "webNavigation", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_TABS, PermissionMessage::kTabs },
    { APIPermission::kWebRequest, "webRequest" },
    { APIPermission::kWebRequestBlocking, "webRequestBlocking" },
    { APIPermission::kWebView, "webview", kFlagCannotBeOptional },

    // Register private permissions.
    { APIPermission::kAutoTestPrivate, "autotestPrivate",
      kFlagCannotBeOptional },
    { APIPermission::kBookmarkManagerPrivate, "bookmarkManagerPrivate",
      kFlagCannotBeOptional },
    { APIPermission::kChromeosInfoPrivate, "chromeosInfoPrivate",
      kFlagCannotBeOptional },
    { APIPermission::kDeveloperPrivate, "developerPrivate",
      kFlagCannotBeOptional },
    { APIPermission::kDial, "dial", kFlagCannotBeOptional },
    { APIPermission::kDownloadsInternal, "downloadsInternal" },
    { APIPermission::kFileBrowserHandlerInternal, "fileBrowserHandlerInternal",
      kFlagCannotBeOptional },
    { APIPermission::kFileBrowserPrivate, "fileBrowserPrivate",
      kFlagCannotBeOptional },
    { APIPermission::kNetworkingPrivate, "networkingPrivate",
      kFlagCannotBeOptional },
    { APIPermission::kManagedModePrivate, "managedModePrivate",
      kFlagCannotBeOptional },
    { APIPermission::kMediaPlayerPrivate, "mediaPlayerPrivate",
      kFlagCannotBeOptional },
    { APIPermission::kMetricsPrivate, "metricsPrivate",
      kFlagCannotBeOptional },
    { APIPermission::kSystemPrivate, "systemPrivate",
      kFlagCannotBeOptional },
    { APIPermission::kCloudPrintPrivate, "cloudPrintPrivate",
      kFlagCannotBeOptional },
    { APIPermission::kInputMethodPrivate, "inputMethodPrivate",
      kFlagCannotBeOptional },
    { APIPermission::kEchoPrivate, "echoPrivate", kFlagCannotBeOptional },
    { APIPermission::kRtcPrivate, "rtcPrivate", kFlagCannotBeOptional },
    { APIPermission::kTerminalPrivate, "terminalPrivate",
      kFlagCannotBeOptional },
    { APIPermission::kWallpaperPrivate, "wallpaperPrivate",
      kFlagCannotBeOptional },
    { APIPermission::kWebRequestInternal, "webRequestInternal" },
    { APIPermission::kWebSocketProxyPrivate, "webSocketProxyPrivate",
      kFlagCannotBeOptional },
    { APIPermission::kWebstorePrivate, "webstorePrivate",
      kFlagCannotBeOptional },
    { APIPermission::kMediaGalleriesPrivate, "mediaGalleriesPrivate",
      kFlagCannotBeOptional },

    // Full url access permissions.
    { APIPermission::kDebugger, "debugger",
      kFlagImpliesFullURLAccess | kFlagCannotBeOptional,
      IDS_EXTENSION_PROMPT_WARNING_DEBUGGER,
      PermissionMessage::kDebugger },
    { APIPermission::kDevtools, "devtools",
      kFlagImpliesFullURLAccess | kFlagCannotBeOptional },
    { APIPermission::kPageCapture, "pageCapture",
      kFlagImpliesFullURLAccess },
    { APIPermission::kTabCapture, "tabCapture",
      kFlagImpliesFullURLAccess },
    { APIPermission::kPlugin, "plugin",
      kFlagImpliesFullURLAccess | kFlagImpliesFullAccess |
          kFlagCannotBeOptional,
      IDS_EXTENSION_PROMPT_WARNING_FULL_ACCESS,
      PermissionMessage::kFullAccess },
    { APIPermission::kProxy, "proxy",
      kFlagImpliesFullURLAccess | kFlagCannotBeOptional },

    // Platform-app permissions.
    { APIPermission::kSerial, "serial", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_SERIAL,
      PermissionMessage::kSerial },
    // Because warning messages for the "socket" permission vary based on the
    // permissions parameters, no message ID or message text is specified here.
    // The message ID and text used will be determined at run-time in the
    // |SocketPermission| class.
    { APIPermission::kSocket, "socket", kFlagCannotBeOptional, 0,
      PermissionMessage::kNone, &::CreateAPIPermission<SocketPermission> },
    { APIPermission::kAppCurrentWindowInternal, "app.currentWindowInternal" },
    { APIPermission::kAppRuntime, "app.runtime" },
    { APIPermission::kAppWindow, "app.window" },
    { APIPermission::kAudioCapture, "audioCapture", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_AUDIO_CAPTURE,
      PermissionMessage::kAudioCapture },
    { APIPermission::kVideoCapture, "videoCapture", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_VIDEO_CAPTURE,
      PermissionMessage::kVideoCapture },
    // The permission string for "fileSystem" is only shown when "write" is
    // present. Read-only access is only granted after the user has been shown
    // a file chooser dialog and selected a file. Selecting the file is
    // considered consent to read it.
    { APIPermission::kFileSystem, "fileSystem" },
    { APIPermission::kFileSystemWrite, "fileSystem.write", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_FILE_SYSTEM_WRITE,
      PermissionMessage::kFileSystemWrite },
    // Because warning messages for the "mediaGalleries" permission vary based
    // on the permissions parameters, no message ID or message text is
    // specified here.
    // The message ID and text used will be determined at run-time in the
    // |MediaGalleriesPermission| class.
    { APIPermission::kMediaGalleries, "mediaGalleries", kFlagNone, 0,
      PermissionMessage::kNone,
      &::CreateAPIPermission<MediaGalleriesPermission> },
    { APIPermission::kPushMessaging, "pushMessaging", kFlagCannotBeOptional },
    { APIPermission::kBluetooth, "bluetooth", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_BLUETOOTH,
      PermissionMessage::kBluetooth },
    { APIPermission::kBluetoothDevice, "bluetoothDevices",
      kFlagNone, 0, PermissionMessage::kNone,
      &::CreateAPIPermission<BluetoothDevicePermission> },
    { APIPermission::kUsb, "usb", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_USB,
      PermissionMessage::kUsb },
    { APIPermission::kUsbDevice, "usbDevices",
      kFlagMustBeOptional, 0, PermissionMessage::kNone,
      &::CreateAPIPermission<UsbDevicePermission> },
    { APIPermission::kSystemIndicator, "systemIndicator", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_SYSTEM_INDICATOR,
      PermissionMessage::kSystemIndicator },
    { APIPermission::kSystemInfoDisplay, "systemInfo.display" },
    { APIPermission::kPointerLock, "pointerLock" },
    { APIPermission::kFullscreen, "fullscreen" },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(PermissionsToRegister); ++i) {
    const PermissionRegistration& pr = PermissionsToRegister[i];
    info->RegisterPermission(
        pr.id, pr.name, pr.l10n_message_id,
        pr.message_id ? pr.message_id : PermissionMessage::kNone,
        pr.flags,
        pr.constructor);
  }

  // Register aliases.
  info->RegisterAlias("unlimitedStorage", kOldUnlimitedStoragePermission);
  info->RegisterAlias("tabs", kWindowsPermission);
}

}  // namespace extensions
