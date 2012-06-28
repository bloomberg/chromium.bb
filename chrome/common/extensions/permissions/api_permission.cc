// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/api_permission.h"

#include "chrome/common/extensions/permissions/permissions_info.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kOldUnlimitedStoragePermission[] = "unlimited_storage";
const char kWindowsPermission[] = "windows";
const char kTemporaryBackgroundAlias[] = "background_alias_do_not_use";

}  // namespace

namespace extensions {

//
// APIPermission
//

APIPermission::~APIPermission() {}

PermissionMessage APIPermission::GetMessage_() const {
  return PermissionMessage(
      message_id_, l10n_util::GetStringUTF16(l10n_message_id_));
}

APIPermission::APIPermission(
    ID id,
    const char* name,
    int l10n_message_id,
    PermissionMessage::ID message_id,
    int flags)
    : id_(id),
      name_(name),
      flags_(flags),
      l10n_message_id_(l10n_message_id),
      message_id_(message_id) {}

// static
void APIPermission::RegisterAllPermissions(
    PermissionsInfo* info) {

  struct PermissionRegistration {
    APIPermission::ID id;
    const char* name;
    int flags;
    int l10n_message_id;
    PermissionMessage::ID message_id;
  } PermissionsToRegister[] = {
    // Register permissions for all extension types.
    { kBackground, "background" },
    { kClipboardRead, "clipboardRead", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_CLIPBOARD,
      PermissionMessage::kClipboard },
    { kClipboardWrite, "clipboardWrite" },
    { kDeclarative, "declarative" },
    { kDeclarativeWebRequest, "declarativeWebRequest" },
    { kDownloads, "downloads", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_DOWNLOADS,
      PermissionMessage::kDownloads },
    { kExperimental, "experimental", kFlagCannotBeOptional },
    { kGeolocation, "geolocation", kFlagCannotBeOptional,
      IDS_EXTENSION_PROMPT_WARNING_GEOLOCATION,
      PermissionMessage::kGeolocation },
    { kNotification, "notifications" },
    { kUnlimitedStorage, "unlimitedStorage", kFlagCannotBeOptional },

    // Register hosted and packaged app permissions.
    { kAppNotifications, "appNotifications" },

    // Register extension permissions.
    { kActiveTab, "activeTab" },
    { kAlarms, "alarms" },
    { kBookmark, "bookmarks", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_BOOKMARKS,
      PermissionMessage::kBookmarks },
    { kBrowsingData, "browsingData" },
    { kContentSettings, "contentSettings", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_CONTENT_SETTINGS,
      PermissionMessage::kContentSettings },
    { kContextMenus, "contextMenus" },
    { kCookie, "cookies" },
    { kFileBrowserHandler, "fileBrowserHandler", kFlagCannotBeOptional },
    { kHistory, "history", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_BROWSING_HISTORY,
      PermissionMessage::kBrowsingHistory },
    { kKeybinding, "keybinding" },
    { kIdle, "idle" },
    { kInput, "input", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_INPUT,
      PermissionMessage::kInput },
    { kManagement, "management", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_MANAGEMENT,
      PermissionMessage::kManagement },
    { kPageCapture, "pageCapture", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_ALL_PAGES_CONTENT,
      PermissionMessage::kAllPageContent },
    { kPrivacy, "privacy", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_PRIVACY,
      PermissionMessage::kPrivacy },
    { kStorage, "storage" },
    { kTab, "tabs", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_TABS,
      PermissionMessage::kTabs },
    { kTopSites, "topSites", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_BROWSING_HISTORY,
      PermissionMessage::kBrowsingHistory },
    { kTts, "tts", 0, kFlagCannotBeOptional },
    { kTtsEngine, "ttsEngine", kFlagCannotBeOptional,
      IDS_EXTENSION_PROMPT_WARNING_TTS_ENGINE,
      PermissionMessage::kTtsEngine },
    { kUsb, "usb", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_USB,
      PermissionMessage::kNone },
    { kWebNavigation, "webNavigation", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_TABS, PermissionMessage::kTabs },
    { kWebRequest, "webRequest" },
    { kWebRequestBlocking, "webRequestBlocking" },

    // Register private permissions.
    { kChromeosInfoPrivate, "chromeosInfoPrivate", kFlagCannotBeOptional },
    { kFileBrowserHandlerInternal, "fileBrowserHandlerInternal",
      kFlagCannotBeOptional },
    { kFileBrowserPrivate, "fileBrowserPrivate", kFlagCannotBeOptional },
    { kManagedModePrivate, "managedModePrivate", kFlagCannotBeOptional },
    { kMediaPlayerPrivate, "mediaPlayerPrivate", kFlagCannotBeOptional },
    { kMetricsPrivate, "metricsPrivate", kFlagCannotBeOptional },
    { kSystemPrivate, "systemPrivate", kFlagCannotBeOptional },
    { kChromeAuthPrivate, "chromeAuthPrivate", kFlagCannotBeOptional },
    { kInputMethodPrivate, "inputMethodPrivate", kFlagCannotBeOptional },
    { kEchoPrivate, "echoPrivate", kFlagCannotBeOptional },
    { kTerminalPrivate, "terminalPrivate", kFlagCannotBeOptional },
    { kWebRequestInternal, "webRequestInternal", kFlagCannotBeOptional },
    { kWebSocketProxyPrivate, "webSocketProxyPrivate", kFlagCannotBeOptional },
    { kWebstorePrivate, "webstorePrivate", kFlagCannotBeOptional },

    // Full url access permissions.
    { kProxy, "proxy", kFlagImpliesFullURLAccess | kFlagCannotBeOptional },
    { kDebugger, "debugger", kFlagImpliesFullURLAccess | kFlagCannotBeOptional,
      IDS_EXTENSION_PROMPT_WARNING_DEBUGGER,
      PermissionMessage::kDebugger },
    { kDevtools, "devtools",
      kFlagImpliesFullURLAccess | kFlagCannotBeOptional },
    { kPlugin, "plugin",
      kFlagImpliesFullURLAccess | kFlagImpliesFullAccess |
          kFlagCannotBeOptional,
      IDS_EXTENSION_PROMPT_WARNING_FULL_ACCESS,
      PermissionMessage::kFullAccess },

    // Platform-app permissions.
    { kSocket, "socket", kFlagCannotBeOptional },
    { kAppWindow, "appWindow" },
    { kAudioCapture, "audioCapture", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_AUDIO_CAPTURE,
      PermissionMessage::kAudioCapture },
    { kVideoCapture, "videoCapture", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_VIDEO_CAPTURE,
      PermissionMessage::kVideoCapture },
    { kFileSystem, "fileSystem" },
    { kFileSystemWrite, "fileSystemWrite", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_FILE_SYSTEM_WRITE,
      PermissionMessage::kFileSystemWrite },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(PermissionsToRegister); ++i) {
    const PermissionRegistration& pr = PermissionsToRegister[i];
    info->RegisterPermission(
        pr.id, pr.name, pr.l10n_message_id,
        pr.message_id ? pr.message_id : PermissionMessage::kNone,
        pr.flags);
  }

  // Register aliases.
  info->RegisterAlias("unlimitedStorage", kOldUnlimitedStoragePermission);
  info->RegisterAlias("tabs", kWindowsPermission);
  // TODO(mihaip): Should be removed for the M20 branch, see
  // http://crbug.com/120447 for more details.
  info->RegisterAlias("background", kTemporaryBackgroundAlias);
}

}  // namespace extensions
