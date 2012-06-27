// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_API_PERMISSION_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_API_PERMISSION_H_
#pragma once

#include <set>

#include "chrome/common/extensions/permissions/permission_message.h"

namespace extensions {

class PermissionsInfo;

// The APIPermission is an immutable class that describes a single
// named permission (API permission).
class APIPermission {
 public:
  enum ID {
    // Error codes.
    kInvalid = -2,
    kUnknown = -1,

    // Real permissions.
    kActiveTab,
    kAlarms,
    kAppNotifications,
    kAppWindow,
    kAudioCapture,
    kBackground,
    kBookmark,
    kBrowsingData,
    kChromeAuthPrivate,
    kChromeosInfoPrivate,
    kClipboardRead,
    kClipboardWrite,
    kContentSettings,
    kContextMenus,
    kCookie,
    kDebugger,
    kDeclarative,
    kDeclarativeWebRequest,
    kDevtools,
    kEchoPrivate,
    kDownloads,
    kExperimental,
    kFileBrowserHandler,
    kFileBrowserHandlerInternal,
    kFileBrowserPrivate,
    kFileSystem,
    kGeolocation,
    kHistory,
    kIdle,
    kInput,
    kInputMethodPrivate,
    kKeybinding,
    kManagedModePrivate,
    kManagement,
    kMediaPlayerPrivate,
    kMetricsPrivate,
    kNotification,
    kPageCapture,
    kPlugin,
    kPrivacy,
    kProxy,
    kSocket,
    kStorage,
    kSystemPrivate,
    kTab,
    kTerminalPrivate,
    kTopSites,
    kTts,
    kTtsEngine,
    kUnlimitedStorage,
    kUsb,
    kVideoCapture,
    kWebNavigation,
    kWebRequest,
    kWebRequestBlocking,
    kWebRequestInternal,
    kWebSocketProxyPrivate,
    kWebstorePrivate,
    kEnumBoundary
  };

  enum Flag {
    kFlagNone = 0,

    // Indicates if the permission implies full access (native code).
    kFlagImpliesFullAccess = 1 << 0,

    // Indicates if the permission implies full URL access.
    kFlagImpliesFullURLAccess = 1 << 1,

    // Indicates that extensions cannot specify the permission as optional.
    kFlagCannotBeOptional = 1 << 3
  };

  typedef std::set<ID> IDSet;

  ~APIPermission();

  // Returns the localized permission message associated with this api.
  // Use GetMessage_ to avoid name conflict with macro GetMessage on Windows.
  PermissionMessage GetMessage_() const;

  int flags() const { return flags_; }

  ID id() const { return id_; }

  // Returns the message id associated with this permission.
  PermissionMessage::ID message_id() const {
    return message_id_;
  }

  // Returns the name of this permission.
  const char* name() const { return name_; }

  // Returns true if this permission implies full access (e.g., native code).
  bool implies_full_access() const {
    return (flags_ & kFlagImpliesFullAccess) != 0;
  }

  // Returns true if this permission implies full URL access.
  bool implies_full_url_access() const {
    return (flags_ & kFlagImpliesFullURLAccess) != 0;
  }

  // Returns true if this permission can be added and removed via the
  // optional permissions extension API.
  bool supports_optional() const {
    return (flags_ & kFlagCannotBeOptional) == 0;
  }

 private:
  // Instances should only be constructed from within PermissionsInfo.
  friend class PermissionsInfo;

  explicit APIPermission(
      ID id,
      const char* name,
      int l10n_message_id,
      PermissionMessage::ID message_id,
      int flags);

  // Register ALL the permissions!
  static void RegisterAllPermissions(PermissionsInfo* info);

  ID id_;
  const char* name_;
  int flags_;
  int l10n_message_id_;
  PermissionMessage::ID message_id_;
};

typedef std::set<APIPermission::ID> APIPermissionSet;

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_API_PERMISSION_H_
