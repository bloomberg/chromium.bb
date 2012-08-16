// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_API_PERMISSION_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_API_PERMISSION_H_

#include <map>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/pickle.h"
#include "chrome/common/extensions/permissions/permission_message.h"

namespace base {
class Value;
}

namespace IPC {
class Message;
}

namespace extensions {

class APIPermissionDetail;
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
    kAppRuntime,
    kAppWindow,
    kAudioCapture,
    kBackground,
    kBookmark,
    kBrowserTag,
    kBrowsingData,
    kChromeosInfoPrivate,
    kClipboardRead,
    kClipboardWrite,
    kCloudPrintPrivate,
    kCommands,
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
    kFileSystemWrite,
    kFontSettings,
    kGeolocation,
    kHistory,
    kIdle,
    kInput,
    kInputMethodPrivate,
    kManagedModePrivate,
    kManagement,
    kMediaGalleries,
    kMediaGalleriesAllGalleries,
    kMediaGalleriesRead,
    kMediaPlayerPrivate,
    kMetricsPrivate,
    kNotification,
    kPageCapture,
    kPlugin,
    kPrivacy,
    kProxy,
    kSerial,
    kSocket,
    kStorage,
    kSystemPrivate,
    kTab,
    kTerminalPrivate,
    kTopSites,
    kTts,
    kTtsEngine,
    kUnlimitedStorage,
    kVideoCapture,
    kWallpaperPrivate,
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

  typedef APIPermissionDetail* (*DetailConstructor)(const APIPermission*);

  typedef std::set<ID> IDSet;

  ~APIPermission();

  // Creates a permission detail instance.
  scoped_refptr<APIPermissionDetail> CreateDetail() const;

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
      int flags,
      DetailConstructor detail_constructor);

  // Register ALL the permissions!
  static void RegisterAllPermissions(PermissionsInfo* info);

  const ID id_;
  const char* const name_;
  const int flags_;
  const int l10n_message_id_;
  const PermissionMessage::ID message_id_;
  const DetailConstructor detail_constructor_;
};

// TODO(penghuang): Rename APIPermissionDetail to APIPermission,
// and APIPermssion to APIPermissionInfo.
class APIPermissionDetail : public base::RefCounted<APIPermissionDetail> {
 public:
  struct CheckParam {
  };

  explicit APIPermissionDetail(const APIPermission* permission)
    : permission_(permission) {
    DCHECK(permission);
  }

  // Returns the id of this permission.
  APIPermission::ID id() const {
    return permission()->id();
  }

  // Returns the name of this permission.
  const char* name() const {
    return permission()->name();
  }

  // Returns the APIPermission of this permission.
  const APIPermission* permission() const {
    return permission_;
  }

  // Returns true if the given permission detail is allowed.
  virtual bool Check(const CheckParam* param) const = 0;

  // Returns true if |detail| is a subset of this.
  virtual bool Contains(const APIPermissionDetail* detail) const = 0;

  // Returns true if |detail| is equal to this.
  virtual bool Equal(const APIPermissionDetail* detail) const = 0;

  // Parses the detail from |value|. Returns false if error happens.
  virtual bool FromValue(const base::Value* value) = 0;

  // Stores this into a new created |value|.
  virtual void ToValue(base::Value** value) const = 0;

  // Clones this.
  virtual APIPermissionDetail* Clone() const = 0;

  // Returns a new API permission detail which equals this - |detail|.
  virtual APIPermissionDetail* Diff(
      const APIPermissionDetail* detail) const = 0;

  // Returns a new API permission detail which equals the union of this and
  // |detail|.
  virtual APIPermissionDetail* Union(
      const APIPermissionDetail* detail) const = 0;

  // Returns a new API permission detail which equals the intersect of this and
  // |detail|.
  virtual APIPermissionDetail* Intersect(
      const APIPermissionDetail* detail) const = 0;

  // IPC functions
  // Writes this into the given IPC message |m|.
  virtual void Write(IPC::Message* m) const = 0;

  // Reads from the given IPC message |m|.
  virtual bool Read(const IPC::Message* m, PickleIterator* iter) = 0;

  // Logs this detail.
  virtual void Log(std::string* log) const = 0;

 protected:
  friend class base::RefCounted<APIPermissionDetail>;
  virtual ~APIPermissionDetail();

 private:
  const APIPermission* const permission_;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_API_PERMISSION_H_
