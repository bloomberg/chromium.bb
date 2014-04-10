// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/chrome_api_permissions.h"

#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/media_galleries_permission.h"
#include "extensions/common/permissions/permission_message.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/permissions/socket_permission.h"
#include "extensions/common/permissions/usb_device_permission.h"
#include "grit/generated_resources.h"

namespace extensions {

namespace {

const char kOldAlwaysOnTopWindowsPermission[] = "alwaysOnTopWindows";
const char kOldFullscreenPermission[] = "fullscreen";
const char kOldOverrideEscFullscreenPermission[] = "overrideEscFullscreen";
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
        {APIPermission::kBackground, "background"},
        {APIPermission::kClipboardRead, "clipboardRead",
         APIPermissionInfo::kFlagNone, IDS_EXTENSION_PROMPT_WARNING_CLIPBOARD,
         PermissionMessage::kClipboard},
        {APIPermission::kClipboardWrite, "clipboardWrite"},
        {APIPermission::kDeclarativeContent, "declarativeContent"},
        {APIPermission::kDeclarativeWebRequest, "declarativeWebRequest",
         APIPermissionInfo::kFlagNone,
         IDS_EXTENSION_PROMPT_WARNING_DECLARATIVE_WEB_REQUEST,
         PermissionMessage::kDeclarativeWebRequest},
        {APIPermission::kDesktopCapture, "desktopCapture",
         APIPermissionInfo::kFlagNone,
         IDS_EXTENSION_PROMPT_WARNING_DESKTOP_CAPTURE,
         PermissionMessage::kDesktopCapture},
        {APIPermission::kDns, "dns"},
        {APIPermission::kDownloads, "downloads", APIPermissionInfo::kFlagNone,
         IDS_EXTENSION_PROMPT_WARNING_DOWNLOADS, PermissionMessage::kDownloads},
        {APIPermission::kDownloadsOpen, "downloads.open",
         APIPermissionInfo::kFlagNone,
         IDS_EXTENSION_PROMPT_WARNING_DOWNLOADS_OPEN,
         PermissionMessage::kDownloadsOpen},
        {APIPermission::kDownloadsShelf, "downloads.shelf"},
        {APIPermission::kIdentity, "identity"},
        {APIPermission::kExperimental, "experimental",
         APIPermissionInfo::kFlagCannotBeOptional},
        // NOTE(kalman): this is provided by a manifest property but needs to
        // appear in the install permission dialogue, so we need a fake
        // permission for it. See http://crbug.com/247857.
        {APIPermission::kWebConnectable, "webConnectable",
         APIPermissionInfo::kFlagCannotBeOptional |
             APIPermissionInfo::kFlagInternal,
         IDS_EXTENSION_PROMPT_WARNING_WEB_CONNECTABLE,
         PermissionMessage::kWebConnectable},
        {APIPermission::kGeolocation, "geolocation",
         APIPermissionInfo::kFlagCannotBeOptional,
         IDS_EXTENSION_PROMPT_WARNING_GEOLOCATION,
         PermissionMessage::kGeolocation},
        {APIPermission::kNotification, "notifications"},
        {APIPermission::kUnlimitedStorage, "unlimitedStorage",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kGcm, "gcm"},

        // Register extension permissions.
        {APIPermission::kAccessibilityFeaturesModify,
         "accessibilityFeatures.modify", APIPermissionInfo::kFlagNone,
         IDS_EXTENSION_PROMPT_WARNING_ACCESSIBILITY_FEATURES_MODIFY,
         PermissionMessage::kAccessibilityFeaturesModify},
        {APIPermission::kAccessibilityFeaturesRead,
         "accessibilityFeatures.read", APIPermissionInfo::kFlagNone,
         IDS_EXTENSION_PROMPT_WARNING_ACCESSIBILITY_FEATURES_READ,
         PermissionMessage::kAccessibilityFeaturesRead},
        {APIPermission::kAccessibilityPrivate, "accessibilityPrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kActiveTab, "activeTab"},
        {APIPermission::kAdView, "adview"},
        {APIPermission::kAlarms, "alarms"},
        {APIPermission::kAutomation, "automation",
         APIPermissionInfo::kFlagCannotBeOptional,
         IDS_EXTENSION_PROMPT_WARNING_AUTOMATION,
         PermissionMessage::kAutomation},
        {APIPermission::kBookmark, "bookmarks", APIPermissionInfo::kFlagNone,
         IDS_EXTENSION_PROMPT_WARNING_BOOKMARKS, PermissionMessage::kBookmarks},
        {APIPermission::kBrailleDisplayPrivate, "brailleDisplayPrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kBrowsingData, "browsingData"},
        {APIPermission::kContentSettings, "contentSettings",
         APIPermissionInfo::kFlagNone,
         IDS_EXTENSION_PROMPT_WARNING_CONTENT_SETTINGS,
         PermissionMessage::kContentSettings},
        {APIPermission::kContextMenus, "contextMenus"},
        {APIPermission::kCookie, "cookies"},
        {APIPermission::kFileBrowserHandler, "fileBrowserHandler",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kFontSettings, "fontSettings",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kHistory, "history", APIPermissionInfo::kFlagNone,
         IDS_EXTENSION_PROMPT_WARNING_BROWSING_HISTORY,
         PermissionMessage::kBrowsingHistory},
        {APIPermission::kIdltest, "idltest"},
        {APIPermission::kIdle, "idle"},
        {APIPermission::kInfobars, "infobars"},
        {APIPermission::kInput, "input", APIPermissionInfo::kFlagNone,
         IDS_EXTENSION_PROMPT_WARNING_INPUT, PermissionMessage::kInput},
        {APIPermission::kLocation, "location",
         APIPermissionInfo::kFlagCannotBeOptional,
         IDS_EXTENSION_PROMPT_WARNING_GEOLOCATION,
         PermissionMessage::kGeolocation},
        {APIPermission::kManagement, "management", APIPermissionInfo::kFlagNone,
         IDS_EXTENSION_PROMPT_WARNING_MANAGEMENT,
         PermissionMessage::kManagement},
        {APIPermission::kNativeMessaging, "nativeMessaging",
         APIPermissionInfo::kFlagNone,
         IDS_EXTENSION_PROMPT_WARNING_NATIVE_MESSAGING,
         PermissionMessage::kNativeMessaging},
        {APIPermission::kPower, "power", },
        {APIPermission::kPrivacy, "privacy", APIPermissionInfo::kFlagNone,
         IDS_EXTENSION_PROMPT_WARNING_PRIVACY, PermissionMessage::kPrivacy},
        {APIPermission::kProcesses, "processes", APIPermissionInfo::kFlagNone,
         IDS_EXTENSION_PROMPT_WARNING_TABS, PermissionMessage::kTabs},
        {APIPermission::kSessions, "sessions"},
        {APIPermission::kSignedInDevices, "signedInDevices",
         APIPermissionInfo::kFlagNone,
         IDS_EXTENSION_PROMPT_WARNING_SIGNED_IN_DEVICES,
         PermissionMessage::kSignedInDevices},
        {APIPermission::kStorage, "storage"},
        {APIPermission::kSyncFileSystem, "syncFileSystem",
         APIPermissionInfo::kFlagNone,
         IDS_EXTENSION_PROMPT_WARNING_SYNCFILESYSTEM,
         PermissionMessage::kSyncFileSystem},
        {APIPermission::kTab, "tabs", APIPermissionInfo::kFlagNone,
         IDS_EXTENSION_PROMPT_WARNING_TABS, PermissionMessage::kTabs},
        {APIPermission::kTopSites, "topSites", APIPermissionInfo::kFlagNone,
         IDS_EXTENSION_PROMPT_WARNING_BROWSING_HISTORY,
         PermissionMessage::kBrowsingHistory},
        {APIPermission::kTts, "tts", 0,
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kTtsEngine, "ttsEngine",
         APIPermissionInfo::kFlagCannotBeOptional,
         IDS_EXTENSION_PROMPT_WARNING_TTS_ENGINE,
         PermissionMessage::kTtsEngine},
        {APIPermission::kWallpaper, "wallpaper",
         APIPermissionInfo::kFlagCannotBeOptional,
         IDS_EXTENSION_PROMPT_WARNING_WALLPAPER, PermissionMessage::kWallpaper},
        {APIPermission::kWebNavigation, "webNavigation",
         APIPermissionInfo::kFlagNone, IDS_EXTENSION_PROMPT_WARNING_TABS,
         PermissionMessage::kTabs},
        {APIPermission::kWebRequest, "webRequest"},
        {APIPermission::kWebRequestBlocking, "webRequestBlocking"},
        {APIPermission::kWebView, "webview",
         APIPermissionInfo::kFlagCannotBeOptional},

        // Register private permissions.
        {APIPermission::kScreenlockPrivate, "screenlockPrivate",
         APIPermissionInfo::kFlagCannotBeOptional,
         IDS_EXTENSION_PROMPT_WARNING_SCREENLOCK_PRIVATE,
         PermissionMessage::kScreenlockPrivate},
        {APIPermission::kActivityLogPrivate, "activityLogPrivate",
         APIPermissionInfo::kFlagCannotBeOptional,
         IDS_EXTENSION_PROMPT_WARNING_ACTIVITY_LOG_PRIVATE,
         PermissionMessage::kActivityLogPrivate},
        {APIPermission::kAutoTestPrivate, "autotestPrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kBookmarkManagerPrivate, "bookmarkManagerPrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kCast, "cast",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kChromeosInfoPrivate, "chromeosInfoPrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kCommandLinePrivate, "commandLinePrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kDeveloperPrivate, "developerPrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kDiagnostics, "diagnostics",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kDial, "dial",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kDownloadsInternal, "downloadsInternal"},
        {APIPermission::kFileBrowserHandlerInternal,
         "fileBrowserHandlerInternal",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kFileBrowserPrivate, "fileBrowserPrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kHotwordPrivate, "hotwordPrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kIdentityPrivate, "identityPrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kLogPrivate, "logPrivate"},
        {APIPermission::kNetworkingPrivate, "networkingPrivate",
         APIPermissionInfo::kFlagCannotBeOptional,
         IDS_EXTENSION_PROMPT_WARNING_NETWORKING_PRIVATE,
         PermissionMessage::kNetworkingPrivate},
        {APIPermission::kMediaPlayerPrivate, "mediaPlayerPrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kMetricsPrivate, "metricsPrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kMDns, "mdns",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kMusicManagerPrivate, "musicManagerPrivate",
         APIPermissionInfo::kFlagCannotBeOptional,
         IDS_EXTENSION_PROMPT_WARNING_MUSIC_MANAGER_PRIVATE,
         PermissionMessage::kMusicManagerPrivate},
        {APIPermission::kPreferencesPrivate, "preferencesPrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kSystemPrivate, "systemPrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kCloudPrintPrivate, "cloudPrintPrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kInputMethodPrivate, "inputMethodPrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kEchoPrivate, "echoPrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kFeedbackPrivate, "feedbackPrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kImageWriterPrivate, "imageWriterPrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kReadingListPrivate, "readingListPrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kRtcPrivate, "rtcPrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kTerminalPrivate, "terminalPrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kVirtualKeyboardPrivate, "virtualKeyboardPrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kWallpaperPrivate, "wallpaperPrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kWebstorePrivate, "webstorePrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kMediaGalleriesPrivate, "mediaGalleriesPrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kStreamsPrivate, "streamsPrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kEnterprisePlatformKeysPrivate,
         "enterprise.platformKeysPrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kWebrtcAudioPrivate, "webrtcAudioPrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kWebrtcLoggingPrivate, "webrtcLoggingPrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kPrincipalsPrivate, "principalsPrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kFirstRunPrivate, "firstRunPrivate",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kBluetoothPrivate, "bluetoothPrivate",
         APIPermissionInfo::kFlagCannotBeOptional,
         IDS_EXTENSION_PROMPT_WARNING_BLUETOOTH_PRIVATE,
         PermissionMessage::kBluetoothPrivate},

        // Full url access permissions.
        {APIPermission::kDebugger, "debugger",
         APIPermissionInfo::kFlagImpliesFullURLAccess |
             APIPermissionInfo::kFlagCannotBeOptional,
         IDS_EXTENSION_PROMPT_WARNING_DEBUGGER, PermissionMessage::kDebugger},
        {APIPermission::kDevtools, "devtools",
         APIPermissionInfo::kFlagImpliesFullURLAccess |
             APIPermissionInfo::kFlagCannotBeOptional |
             APIPermissionInfo::kFlagInternal},
        {APIPermission::kPageCapture, "pageCapture",
         APIPermissionInfo::kFlagImpliesFullURLAccess},
        {APIPermission::kTabCapture, "tabCapture",
         APIPermissionInfo::kFlagImpliesFullURLAccess},
        {APIPermission::kTabCaptureForTab, "tabCaptureForTab",
         APIPermissionInfo::kFlagInternal},
        {APIPermission::kPlugin, "plugin",
         APIPermissionInfo::kFlagImpliesFullURLAccess |
             APIPermissionInfo::kFlagImpliesFullAccess |
             APIPermissionInfo::kFlagCannotBeOptional |
             APIPermissionInfo::kFlagInternal,
         IDS_EXTENSION_PROMPT_WARNING_FULL_ACCESS,
         PermissionMessage::kFullAccess},
        {APIPermission::kProxy, "proxy",
         APIPermissionInfo::kFlagImpliesFullURLAccess |
             APIPermissionInfo::kFlagCannotBeOptional},

        // Platform-app permissions.
        {APIPermission::kSerial, "serial", APIPermissionInfo::kFlagNone,
         IDS_EXTENSION_PROMPT_WARNING_SERIAL, PermissionMessage::kSerial},
        // Because warning messages for the "socket" permission vary based on
        // the
        // permissions parameters, no message ID or message text is specified
        // here.
        // The message ID and text used will be determined at run-time in the
        // |SocketPermission| class.
        {APIPermission::kSocket, "socket",
         APIPermissionInfo::kFlagCannotBeOptional, 0, PermissionMessage::kNone,
         &CreateAPIPermission<SocketPermission>},
        {APIPermission::kAlwaysOnTopWindows, "app.window.alwaysOnTop"},
        {APIPermission::kAudioCapture, "audioCapture",
         APIPermissionInfo::kFlagNone,
         IDS_EXTENSION_PROMPT_WARNING_AUDIO_CAPTURE,
         PermissionMessage::kAudioCapture},
        {APIPermission::kVideoCapture, "videoCapture",
         APIPermissionInfo::kFlagNone,
         IDS_EXTENSION_PROMPT_WARNING_VIDEO_CAPTURE,
         PermissionMessage::kVideoCapture},
        // The permission string for "fileSystem" is only shown when "write" or
        // "directory" is present. Read-only access is only granted after the
        // user
        // has been shown a file or directory  chooser dialog and selected a
        // file or
        // directory . Selecting the file or directory  is considered consent to
        // read it.
        {APIPermission::kFileSystem, "fileSystem"},
        {APIPermission::kFileSystemDirectory, "fileSystem.directory",
         APIPermissionInfo::kFlagNone,
         IDS_EXTENSION_PROMPT_WARNING_FILE_SYSTEM_DIRECTORY,
         PermissionMessage::kFileSystemDirectory},
        {APIPermission::kFileSystemProvider, "fileSystemProvider"},
        {APIPermission::kFileSystemRetainEntries, "fileSystem.retainEntries"},
        {APIPermission::kFileSystemWrite, "fileSystem.write",
         APIPermissionInfo::kFlagNone,
         IDS_EXTENSION_PROMPT_WARNING_FILE_SYSTEM_WRITE,
         PermissionMessage::kFileSystemWrite},
        {APIPermission::kFileSystemWriteDirectory, "fileSystem.writeDirectory",
         APIPermissionInfo::kFlagNone,
         IDS_EXTENSION_PROMPT_WARNING_FILE_SYSTEM_WRITE_DIRECTORY,
         PermissionMessage::kFileSystemWriteDirectory},
        {APIPermission::kHid, "hid", APIPermissionInfo::kFlagNone,
         IDS_EXTENSION_PROMPT_WARNING_HID, PermissionMessage::kHid},
        // Because warning messages for the "mediaGalleries" permission vary
        // based
        // on the permissions parameters, no message ID or message text is
        // specified here.
        // The message ID and text used will be determined at run-time in the
        // |MediaGalleriesPermission| class.
        {APIPermission::kMediaGalleries, "mediaGalleries",
         APIPermissionInfo::kFlagNone, 0, PermissionMessage::kNone,
         &CreateAPIPermission<MediaGalleriesPermission>},
        {APIPermission::kPushMessaging, "pushMessaging",
         APIPermissionInfo::kFlagCannotBeOptional},
        {APIPermission::kUsb, "usb", APIPermissionInfo::kFlagNone,
         IDS_EXTENSION_PROMPT_WARNING_USB, PermissionMessage::kUsb},
        {APIPermission::kUsbDevice, "usbDevices", APIPermissionInfo::kFlagNone,
         0, PermissionMessage::kNone,
         &CreateAPIPermission<UsbDevicePermission>},
        {APIPermission::kSystemIndicator, "systemIndicator",
         APIPermissionInfo::kFlagNone,
         IDS_EXTENSION_PROMPT_WARNING_SYSTEM_INDICATOR,
         PermissionMessage::kSystemIndicator},
        {APIPermission::kSystemCpu, "system.cpu"},
        {APIPermission::kSystemMemory, "system.memory"},
        {APIPermission::kSystemNetwork, "system.network"},
        {APIPermission::kSystemDisplay, "system.display"},
        {APIPermission::kSystemStorage, "system.storage"},
        {APIPermission::kPointerLock, "pointerLock"},
        {APIPermission::kFullscreen, "app.window.fullscreen"},
        {APIPermission::kAudio, "audio"},
        {APIPermission::kCastStreaming, "cast.streaming"},
        {APIPermission::kOverrideEscFullscreen,
         "app.window.fullscreen.overrideEsc"},
        {APIPermission::kWindowShape, "app.window.shape"},

        // Settings override permissions.
        {APIPermission::kHomepage, "homepage",
         APIPermissionInfo::kFlagCannotBeOptional |
             APIPermissionInfo::kFlagInternal,
         IDS_EXTENSION_PROMPT_WARNING_HOME_PAGE_SETTING_OVERRIDE,
         PermissionMessage::kHomepage},
        {APIPermission::kSearchProvider, "searchProvider",
         APIPermissionInfo::kFlagCannotBeOptional |
             APIPermissionInfo::kFlagInternal,
         IDS_EXTENSION_PROMPT_WARNING_SEARCH_SETTINGS_OVERRIDE,
         PermissionMessage::kSearchProvider},
        {APIPermission::kStartupPages, "startupPages",
         APIPermissionInfo::kFlagCannotBeOptional |
             APIPermissionInfo::kFlagInternal,
         IDS_EXTENSION_PROMPT_WARNING_START_PAGE_SETTING_OVERRIDE,
         PermissionMessage::kStartupPages}, };

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

std::vector<PermissionsProvider::AliasInfo>
ChromeAPIPermissions::GetAllAliases() const {
  // Register aliases.
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
  aliases.push_back(PermissionsProvider::AliasInfo(
      "tabs", kWindowsPermission));
  return aliases;
}

}  // namespace extensions
