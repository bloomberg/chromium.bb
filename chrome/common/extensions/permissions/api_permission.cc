// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/api_permission.h"

#include "chrome/common/extensions/permissions/permissions_info.h"
#include "chrome/common/extensions/permissions/socket_permission.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using extensions::APIPermission;
using extensions::APIPermissionDetail;

const char kOldUnlimitedStoragePermission[] = "unlimited_storage";
const char kWindowsPermission[] = "windows";

class SimpleDetail : public APIPermissionDetail {
 public:
  explicit SimpleDetail(const APIPermission* permission)
    : APIPermissionDetail(permission) { }

  virtual bool FromValue(const base::Value* value) OVERRIDE {
    if (value)
      return false;
    return true;
  }

  virtual void ToValue(base::Value** value) const OVERRIDE {
    *value = NULL;
  }

  virtual bool Check(
      const APIPermissionDetail::CheckParam* param) const OVERRIDE {
    return !param;
  }

  virtual bool Equal(const APIPermissionDetail* detail) const OVERRIDE {
    if (this == detail)
      return true;
    CHECK(permission() == detail->permission());
    return true;
  }

  virtual APIPermissionDetail* Clone() const OVERRIDE {
    return new SimpleDetail(permission());
  }

  virtual APIPermissionDetail* Diff(
      const APIPermissionDetail* detail) const OVERRIDE {
    CHECK(permission() == detail->permission());
    return NULL;
  }

  virtual APIPermissionDetail* Union(
      const APIPermissionDetail* detail) const OVERRIDE {
    CHECK(permission() == detail->permission());
    return new SimpleDetail(permission());
  }

  virtual APIPermissionDetail* Intersect(
      const APIPermissionDetail* detail) const OVERRIDE {
    CHECK(permission() == detail->permission());
    return new SimpleDetail(permission());
  }

  virtual bool Contains(const APIPermissionDetail* detail) const OVERRIDE {
    CHECK(permission() == detail->permission());
    return true;
  }

  virtual void Write(IPC::Message* m) const OVERRIDE { }

  virtual bool Read(const IPC::Message* m, PickleIterator* iter) OVERRIDE {
    return true;
  }

  virtual void Log(std::string* log) const OVERRIDE { }

 protected:
  friend class extensions::APIPermissionDetail;
  virtual ~SimpleDetail() { }
};

template<typename T>
APIPermissionDetail* CreatePermissionDetail(const APIPermission* permission) {
  return new T(permission);
}

}  // namespace

namespace extensions {

//
// APIPermission
//

APIPermission::~APIPermission() {}

scoped_refptr<APIPermissionDetail> APIPermission::CreateDetail() const {
  scoped_refptr<APIPermissionDetail> p;
  if (detail_constructor_)
    p = detail_constructor_(this);
  if (!p.get())
    p = new SimpleDetail(this);
  return p;
}

PermissionMessage APIPermission::GetMessage_() const {
  return PermissionMessage(
      message_id_, l10n_util::GetStringUTF16(l10n_message_id_));
}

APIPermission::APIPermission(
    ID id,
    const char* name,
    int l10n_message_id,
    PermissionMessage::ID message_id,
    int flags,
    DetailConstructor detail_constructor)
    : id_(id),
      name_(name),
      flags_(flags),
      l10n_message_id_(l10n_message_id),
      message_id_(message_id),
      detail_constructor_(detail_constructor) { }

// static
void APIPermission::RegisterAllPermissions(
    PermissionsInfo* info) {

  struct PermissionRegistration {
    APIPermission::ID id;
    const char* name;
    int flags;
    int l10n_message_id;
    PermissionMessage::ID message_id;
    DetailConstructor detail_constructor;
  } PermissionsToRegister[] = {
    // Register permissions for all extension types.
    { kBackground, "background" },
    { kClipboardRead, "clipboardRead", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_CLIPBOARD,
      PermissionMessage::kClipboard },
    { kClipboardWrite, "clipboardWrite" },
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
    { kBrowserTag, "browserTag", kFlagCannotBeOptional },
    { kBrowsingData, "browsingData" },
    { kCommands, "commands" },
    { kContentSettings, "contentSettings", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_CONTENT_SETTINGS,
      PermissionMessage::kContentSettings },
    { kContextMenus, "contextMenus" },
    { kCookie, "cookies" },
    { kFileBrowserHandler, "fileBrowserHandler", kFlagCannotBeOptional },
    { kFontSettings, "fontSettings", kFlagCannotBeOptional },
    { kHistory, "history", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_BROWSING_HISTORY,
      PermissionMessage::kBrowsingHistory },
    { kIdle, "idle" },
    { kInput, "input", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_INPUT,
      PermissionMessage::kInput },
    { kManagement, "management", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_MANAGEMENT,
      PermissionMessage::kManagement },
    { kMediaGalleries, "mediaGalleries" },
    { kMediaGalleriesRead, "mediaGalleriesRead" },
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
    { kCloudPrintPrivate, "cloudPrintPrivate", kFlagCannotBeOptional },
    { kInputMethodPrivate, "inputMethodPrivate", kFlagCannotBeOptional },
    { kEchoPrivate, "echoPrivate", kFlagCannotBeOptional },
    { kTerminalPrivate, "terminalPrivate", kFlagCannotBeOptional },
    { kWallpaperPrivate, "wallpaperPrivate", kFlagCannotBeOptional },
    { kWebRequestInternal, "webRequestInternal" },
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
    { kSerial, "serial", kFlagCannotBeOptional },
    { kSocket, "socket", kFlagCannotBeOptional, 0, PermissionMessage::kNone,
      &CreatePermissionDetail<SocketPermission> },
    { kAppRuntime, "app.runtime" },
    { kAppWindow, "app.window" },
    { kAudioCapture, "audioCapture", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_AUDIO_CAPTURE,
      PermissionMessage::kAudioCapture },
    { kVideoCapture, "videoCapture", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_VIDEO_CAPTURE,
      PermissionMessage::kVideoCapture },
    // "fileSystem" has no permission string because read-only access is only
    // granted after the user has been shown a file chooser dialog and selected
    // a file. Selecting the file is considered consent to read it.
    { kFileSystem, "fileSystem" },
    { kFileSystemWrite, "fileSystemWrite", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_FILE_SYSTEM_WRITE,
      PermissionMessage::kFileSystemWrite },
    { kMediaGalleriesAllGalleries, "mediaGalleriesAllGalleries",
      kFlagCannotBeOptional,
      IDS_EXTENSION_PROMPT_WARNING_MEDIA_GALLERIES_ALL_GALLERIES,
      PermissionMessage::kMediaGalleriesAllGalleries },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(PermissionsToRegister); ++i) {
    const PermissionRegistration& pr = PermissionsToRegister[i];
    info->RegisterPermission(
        pr.id, pr.name, pr.l10n_message_id,
        pr.message_id ? pr.message_id : PermissionMessage::kNone,
        pr.flags,
        pr.detail_constructor);
  }

  // Register aliases.
  info->RegisterAlias("unlimitedStorage", kOldUnlimitedStoragePermission);
  info->RegisterAlias("tabs", kWindowsPermission);
}

APIPermissionDetail::~APIPermissionDetail() {
}

}  // namespace extensions
