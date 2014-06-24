// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_PERMISSION_MESSAGE_H_
#define EXTENSIONS_COMMON_PERMISSIONS_PERMISSION_MESSAGE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/strings/string16.h"

namespace extensions {

// When prompting the user to install or approve permissions, we display
// messages describing the effects of the permissions rather than listing the
// permissions themselves. Each PermissionMessage represents one of the
// messages shown to the user.
class PermissionMessage {
 public:
  // Do not reorder this enumeration. If you need to add a new enum, add it just
  // prior to kEnumBoundary.
  enum ID {
    kUnknown,
    kNone,
    kBookmarks,
    kGeolocation,
    kBrowsingHistory,
    kTabs,
    kManagement,
    kDebugger,
    kDesktopCapture,
    kHid,
    kHosts1,
    kHosts2,
    kHosts3,
    kHosts4OrMore,
    kHostsAll,
    kFullAccess,
    kClipboard,
    kTtsEngine,
    kContentSettings,
    kPrivacy,
    kSupervisedUser,
    kInput,
    kAudioCapture,
    kVideoCapture,
    kDownloads,
    kDeleted_FileSystemWrite,
    kMediaGalleriesAllGalleriesRead,
    kSerial,
    kSocketAnyHost,
    kSocketDomainHosts,
    kSocketSpecificHosts,
    kBluetooth,
    kUsb,
    kSystemIndicator,
    kUsbDevice,
    kMediaGalleriesAllGalleriesCopyTo,
    kSystemInfoDisplay,
    kNativeMessaging,
    kSyncFileSystem,
    kAudio,
    kFavicon,
    kMusicManagerPrivate,
    kWebConnectable,
    kActivityLogPrivate,
    kBluetoothDevices,
    kDownloadsOpen,
    kNetworkingPrivate,
    kDeclarativeWebRequest,
    kFileSystemDirectory,
    kFileSystemWriteDirectory,
    kSignedInDevices,
    kWallpaper,
    kNetworkState,
    kHomepage,
    kSearchProvider,
    kStartupPages,
    kMediaGalleriesAllGalleriesDelete,
    kScreenlockPrivate,
    kOverrideBookmarksUI,
    kAutomation,
    kAccessibilityFeaturesModify,
    kAccessibilityFeaturesRead,
    kBluetoothPrivate,
    kIdentityEmail,
    kEnumBoundary,
  };
  COMPILE_ASSERT(PermissionMessage::kNone > PermissionMessage::kUnknown,
                 kNone_not_greater_than_kUnknown);

  // Creates the corresponding permission message.
  PermissionMessage(ID id, const base::string16& message);
  PermissionMessage(ID id,
                    const base::string16& message,
                    const base::string16& details);
  ~PermissionMessage();

  // Gets the id of the permission message, which can be used in UMA
  // histograms.
  ID id() const { return id_; }

  // Gets a localized message describing this permission. Please note that
  // the message will be empty for message types TYPE_NONE and TYPE_UNKNOWN.
  const base::string16& message() const { return message_; }

  // Gets a localized message describing the details for this permission. Please
  // note that the message will be empty for message types TYPE_NONE and
  // TYPE_UNKNOWN.
  const base::string16& details() const { return details_; }

  // Comparator to work with std::set.
  bool operator<(const PermissionMessage& that) const {
    return id_ < that.id_;
  }
  // Comparator to work with base::STLSetDifference.
  bool operator>(const PermissionMessage& that) const {
    return id_ > that.id_;
  }

 private:
  ID id_;
  base::string16 message_;
  base::string16 details_;
};

typedef std::vector<PermissionMessage> PermissionMessages;

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_PERMISSION_MESSAGE_H_
