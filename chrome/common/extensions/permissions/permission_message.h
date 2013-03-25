// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_PERMISSION_MESSAGE_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_PERMISSION_MESSAGE_H_

#include <set>
#include <string>
#include <vector>

#include "base/string16.h"

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
    kManagedMode,
    kInput,
    kAudioCapture,
    kVideoCapture,
    kDownloads,
    kFileSystemWrite,
    kMediaGalleriesAllGalleriesRead,
    kSerial,
    kSocketAnyHost,
    kSocketDomainHosts,
    kSocketSpecificHosts,
    kBluetooth,
    kUsb,
    kSystemIndicator,
    kBluetoothDevice,
    kUsbDevice,
    kMediaGalleriesAllGalleriesWrite,
    kSystemInfoDisplay,
    kNativeMessaging,
    kSyncFileSystem,
    kEnumBoundary,
  };

  // Creates the corresponding permission message for a list of hosts. This is
  // simply a convenience method around the constructor, since the messages
  // change depending on what hosts are present.
  static PermissionMessage CreateFromHostList(
      const std::set<std::string>& hosts);

  // Creates the corresponding permission message.
  PermissionMessage(ID id, const string16& message);
  ~PermissionMessage();

  // Gets the id of the permission message, which can be used in UMA
  // histograms.
  ID id() const { return id_; }

  // Gets a localized message describing this permission. Please note that
  // the message will be empty for message types TYPE_NONE and TYPE_UNKNOWN.
  const string16& message() const { return message_; }

  // Comparator to work with std::set.
  bool operator<(const PermissionMessage& that) const {
    return id_ < that.id_;
  }

 private:
  ID id_;
  string16 message_;
};

typedef std::vector<PermissionMessage> PermissionMessages;

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_PERMISSION_MESSAGE_H_
