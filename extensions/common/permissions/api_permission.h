// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_API_PERMISSION_H_
#define EXTENSIONS_COMMON_PERMISSIONS_API_PERMISSION_H_

#include <map>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/pickle.h"
#include "base/values.h"
#include "extensions/common/permissions/permission_message.h"

namespace IPC {
class Message;
}

namespace extensions {

class APIPermissionInfo;
class ChromeAPIPermissions;

// APIPermission is for handling some complex permissions. Please refer to
// extensions::SocketPermission as an example.
// There is one instance per permission per loaded extension.
class APIPermission {
 public:
  enum ID {
    // Error codes.
    kInvalid = -2,
    kUnknown = -1,

    // Real permissions.
    kAccessibilityFeaturesModify,
    kAccessibilityFeaturesRead,
    kAccessibilityPrivate,
    kActiveTab,
    kActivityLogPrivate,
    kAlarms,
    kAlwaysOnTopWindows,
    kAppView,
    kAudio,
    kAudioCapture,
    kAutomation,
    kAutoTestPrivate,
    kBackground,
    kBluetoothPrivate,
    kBookmark,
    kBookmarkManagerPrivate,
    kBrailleDisplayPrivate,
    kBrowsingData,
    kCast,
    kCastStreaming,
    kChromeosInfoPrivate,
    kClipboardRead,
    kClipboardWrite,
    kCloudPrintPrivate,
    kCommandLinePrivate,
    kContentSettings,
    kContextMenus,
    kCookie,
    kDiagnostics,
    kDial,
    kDebugger,
    kDeclarative,
    kDeclarativeContent,
    kDeclarativeWebRequest,
    kDesktopCapture,
    kDeveloperPrivate,
    kDevtools,
    kDns,
    kDownloads,
    kDownloadsInternal,
    kDownloadsOpen,
    kDownloadsShelf,
    kEasyUnlockPrivate,
    kEchoPrivate,
    kEmbeddedExtensionOptions,
    kEnterprisePlatformKeys,
    kEnterprisePlatformKeysPrivate,
    kExperienceSamplingPrivate,
    kExperimental,
    kFeedbackPrivate,
    kFileBrowserHandler,
    kFileBrowserHandlerInternal,
    kFileBrowserPrivate,
    kFileSystem,
    kFileSystemDirectory,
    kFileSystemProvider,
    kFileSystemRetainEntries,
    kFileSystemWrite,
    kFileSystemWriteDirectory,
    kFontSettings,
    kFullscreen,
    kGcdPrivate,
    kGcm,
    kGeolocation,
    kHid,
    kHistory,
    kHomepage,
    kHotwordPrivate,
    kIdentity,
    kIdentityEmail,
    kIdentityPrivate,
    kIdltest,
    kIdle,
    kInfobars,
    kInput,
    kInputMethodPrivate,
    kLedger,
    kLocation,
    kLogPrivate,
    kManagement,
    kMediaGalleries,
    kMediaGalleriesPrivate,
    kMediaPlayerPrivate,
    kMetricsPrivate,
    kMDns,
    kMusicManagerPrivate,
    kNativeMessaging,
    kNetworkingPrivate,
    kNotifications,
    kOverrideEscFullscreen,
    kPageCapture,
    kPointerLock,
    kPlugin,
    kPower,
    kPreferencesPrivate,
    kPrincipalsPrivate,
    kPrivacy,
    kProcesses,
    kProxy,
    kPushMessaging,
    kImageWriterPrivate,
    kReadingListPrivate,
    kRtcPrivate,
    kSearchProvider,
    kSerial,
    kSessions,
    kSignedInDevices,
    kSocket,
    kStartupPages,
    kStorage,
    kStreamsPrivate,
    kSyncFileSystem,
    kSyncedNotificationsPrivate,
    kSystemPrivate,
    kSystemDisplay,
    kSystemStorage,
    kTab,
    kTabCapture,
    kTabCaptureForTab,
    kTerminalPrivate,
    kTopSites,
    kTts,
    kTtsEngine,
    kUnlimitedStorage,
    kUsb,
    kUsbDevice,
    kVideoCapture,
    kVirtualKeyboardPrivate,
    kWallpaper,
    kWallpaperPrivate,
    kWebcamPrivate,
    kWebConnectable,  // for externally_connectable manifest key
    kWebNavigation,
    kWebRequest,
    kWebRequestBlocking,
    kWebrtcAudioPrivate,
    kWebrtcLoggingPrivate,
    kWebstorePrivate,
    kWebView,
    kWindowShape,
    kScreenlockPrivate,
    kSystemCpu,
    kSystemMemory,
    kSystemNetwork,
    kSystemInfoCpu,
    kSystemInfoMemory,
    kFirstRunPrivate,
    kBrowser,
    kEnumBoundary
  };

  struct CheckParam {
  };

  explicit APIPermission(const APIPermissionInfo* info);

  virtual ~APIPermission();

  // Returns the id of this permission.
  ID id() const;

  // Returns the name of this permission.
  const char* name() const;

  // Returns the APIPermission of this permission.
  const APIPermissionInfo* info() const {
    return info_;
  }

  // Returns true if this permission has any PermissionMessages.
  virtual bool HasMessages() const = 0;

  // Returns the localized permission messages of this permission.
  virtual PermissionMessages GetMessages() const = 0;

  // Returns true if the given permission is allowed.
  virtual bool Check(const CheckParam* param) const = 0;

  // Returns true if |rhs| is a subset of this.
  virtual bool Contains(const APIPermission* rhs) const = 0;

  // Returns true if |rhs| is equal to this.
  virtual bool Equal(const APIPermission* rhs) const = 0;

  // Parses the APIPermission from |value|. Returns false if an error happens
  // and optionally set |error| if |error| is not NULL. If |value| represents
  // multiple permissions, some are invalid, and |unhandled_permissions| is
  // not NULL, the invalid ones are put into |unhandled_permissions| and the
  // function returns true.
  virtual bool FromValue(const base::Value* value,
                         std::string* error,
                         std::vector<std::string>* unhandled_permissions) = 0;

  // Stores this into a new created |value|.
  virtual scoped_ptr<base::Value> ToValue() const = 0;

  // Clones this.
  virtual APIPermission* Clone() const = 0;

  // Returns a new API permission which equals this - |rhs|.
  virtual APIPermission* Diff(const APIPermission* rhs) const = 0;

  // Returns a new API permission which equals the union of this and |rhs|.
  virtual APIPermission* Union(const APIPermission* rhs) const = 0;

  // Returns a new API permission which equals the intersect of this and |rhs|.
  virtual APIPermission* Intersect(const APIPermission* rhs) const = 0;

  // IPC functions
  // Writes this into the given IPC message |m|.
  virtual void Write(IPC::Message* m) const = 0;

  // Reads from the given IPC message |m|.
  virtual bool Read(const IPC::Message* m, PickleIterator* iter) = 0;

  // Logs this permission.
  virtual void Log(std::string* log) const = 0;

 protected:
  // Returns the localized permission message associated with this api.
  // Use GetMessage_ to avoid name conflict with macro GetMessage on Windows.
  PermissionMessage GetMessage_() const;

 private:
  const APIPermissionInfo* const info_;
};


// The APIPermissionInfo is an immutable class that describes a single
// named permission (API permission).
// There is one instance per permission.
class APIPermissionInfo {
 public:
  enum Flag {
    kFlagNone = 0,

    // Indicates if the permission implies full access (native code).
    kFlagImpliesFullAccess = 1 << 0,

    // Indicates if the permission implies full URL access.
    kFlagImpliesFullURLAccess = 1 << 1,

    // Indicates that extensions cannot specify the permission as optional.
    kFlagCannotBeOptional = 1 << 3,

    // Indicates that the permission is internal to the extensions
    // system and cannot be specified in the "permissions" list.
    kFlagInternal = 1 << 4,
  };

  typedef APIPermission* (*APIPermissionConstructor)(const APIPermissionInfo*);

  typedef std::set<APIPermission::ID> IDSet;

  ~APIPermissionInfo();

  // Creates a APIPermission instance.
  APIPermission* CreateAPIPermission() const;

  int flags() const { return flags_; }

  APIPermission::ID id() const { return id_; }

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

  // Returns true if this permission is internal rather than a
  // "permissions" list entry.
  bool is_internal() const {
    return (flags_ & kFlagInternal) != 0;
  }

 private:
  // Instances should only be constructed from within a PermissionsProvider.
  friend class ChromeAPIPermissions;
  friend class ExtensionsAPIPermissions;
  // Implementations of APIPermission will want to get the permission message,
  // but this class's implementation should be hidden from everyone else.
  friend class APIPermission;

  // This exists to allow aggregate initialization, so that default values
  // for flags, etc. can be omitted.
  // TODO(yoz): Simplify the way initialization is done. APIPermissionInfo
  // should be the simple data struct.
  struct InitInfo {
    APIPermission::ID id;
    const char* name;
    int flags;
    int l10n_message_id;
    PermissionMessage::ID message_id;
    APIPermissionInfo::APIPermissionConstructor constructor;
  };

  explicit APIPermissionInfo(const InitInfo& info);

  // Returns the localized permission message associated with this api.
  // Use GetMessage_ to avoid name conflict with macro GetMessage on Windows.
  PermissionMessage GetMessage_() const;

  const APIPermission::ID id_;
  const char* const name_;
  const int flags_;
  const int l10n_message_id_;
  const PermissionMessage::ID message_id_;
  const APIPermissionConstructor api_permission_constructor_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_API_PERMISSION_H_
