// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_PERMISSION_MESSAGE_PROVIDER_H_
#define EXTENSIONS_COMMON_PERMISSIONS_PERMISSION_MESSAGE_PROVIDER_H_

#include <vector>

#include "extensions/common/manifest.h"
#include "extensions/common/permissions/permission_message.h"

namespace extensions {

class PermissionSet;

// The PermissionMessageProvider interprets permissions, translating them
// into warning messages to show to the user. It also determines whether
// a new set of permissions entails showing new warning messages.
class PermissionMessageProvider {
 public:
  PermissionMessageProvider() {}
  virtual ~PermissionMessageProvider() {}

  // Return the global permission message provider.
  static const PermissionMessageProvider* Get();

  // Gets the localized permission messages that represent this set.
  // The set of permission messages shown varies by extension type.
  virtual PermissionMessages GetPermissionMessages(
      const PermissionSet* permissions,
      Manifest::Type extension_type) const = 0;

  // Gets the localized permission messages that represent this set (represented
  // as strings). The set of permission messages shown varies by extension type.
  virtual std::vector<base::string16> GetWarningMessages(
      const PermissionSet* permissions,
      Manifest::Type extension_type) const = 0;

  // Gets the localized permission details for messages that represent this set
  // (represented as strings). The set of permission messages shown varies by
  // extension type.
  virtual std::vector<base::string16> GetWarningMessagesDetails(
      const PermissionSet* permissions,
      Manifest::Type extension_type) const = 0;

  // Returns true if |new_permissions| has a greater privilege level than
  // |old_permissions|.
  // Whether certain permissions are considered varies by extension type.
  virtual bool IsPrivilegeIncrease(
      const PermissionSet* old_permissions,
      const PermissionSet* new_permissions,
      Manifest::Type extension_type) const = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_PERMISSION_MESSAGE_PROVIDER_H_
