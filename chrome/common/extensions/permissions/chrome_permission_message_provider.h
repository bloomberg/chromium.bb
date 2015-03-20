// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_CHROME_PERMISSION_MESSAGE_PROVIDER_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_CHROME_PERMISSION_MESSAGE_PROVIDER_H_

#include <set>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/strings/string16.h"
#include "extensions/common/permissions/permission_message_provider.h"

namespace extensions {

// Tested in two places:
// 1. chrome_permission_message_provider_unittest.cc, which is a regular unit
//    test for this class
// 2. chrome/browser/extensions/permission_messages_unittest.cc, which is an
//    integration test that ensures messages are correctly generated for
//    extensions created through the extension system.
class ChromePermissionMessageProvider : public PermissionMessageProvider {
 public:
  ChromePermissionMessageProvider();
  ~ChromePermissionMessageProvider() override;

  // PermissionMessageProvider implementation.
  // See comments in permission_message_provider.h. TL;DR: You want to use only
  // GetPermissionMessageStrings to get messages, not the *Legacy* or
  // *Coalesced* methods.
  PermissionMessageIDs GetLegacyPermissionMessageIDs(
      const PermissionSet* permissions,
      Manifest::Type extension_type) const override;
  CoalescedPermissionMessages GetCoalescedPermissionMessages(
      const PermissionIDSet& permissions) const override;
  std::vector<base::string16> GetLegacyWarningMessages(
      const PermissionSet* permissions,
      Manifest::Type extension_type) const override;
  std::vector<base::string16> GetLegacyWarningMessagesDetails(
      const PermissionSet* permissions,
      Manifest::Type extension_type) const override;
  bool IsPrivilegeIncrease(const PermissionSet* old_permissions,
                           const PermissionSet* new_permissions,
                           Manifest::Type extension_type) const override;
  PermissionIDSet GetAllPermissionIDs(
      const PermissionSet* permissions,
      Manifest::Type extension_type) const override;

 private:
  // TODO(treib): Remove this once we've switched to the new system.
  PermissionMessages GetLegacyPermissionMessages(
      const PermissionSet* permissions,
      Manifest::Type extension_type) const;

  // Gets the permission messages for the API permissions. Also adds any
  // permission IDs from API Permissions to |permission_ids|.
  // TODO(sashab): Deprecate the |permissions| argument, and rename this to
  // AddAPIPermissions().
  std::set<PermissionMessage> GetAPIPermissionMessages(
      const PermissionSet* permissions,
      PermissionIDSet* permission_ids,
      Manifest::Type extension_type) const;

  // Gets the permission messages for the Manifest permissions. Also adds any
  // permission IDs from manifest Permissions to |permission_ids|.
  // TODO(sashab): Deprecate the |permissions| argument, and rename this to
  // AddManifestPermissions().
  std::set<PermissionMessage> GetManifestPermissionMessages(
      const PermissionSet* permissions,
      PermissionIDSet* permission_ids) const;

  // Gets the permission messages for the host permissions. Also adds any
  // permission IDs from host Permissions to |permission_ids|.
  // TODO(sashab): Deprecate the |permissions| argument, and rename this to
  // AddHostPermissions().
  std::set<PermissionMessage> GetHostPermissionMessages(
      const PermissionSet* permissions,
      PermissionIDSet* permission_ids,
      Manifest::Type extension_type) const;

  // Applies coalescing rules and writes the resulting messages and their
  // details into |message_strings| and |message_details_strings|.
  // TODO(treib): Remove this method as soon as we've fully switched to the
  // new system.
  void CoalesceWarningMessages(
      const PermissionSet* permissions,
      Manifest::Type extension_type,
      std::vector<base::string16>* message_strings,
      std::vector<base::string16>* message_details_strings) const;

  // Returns true if |new_permissions| has an elevated API privilege level
  // compared to |old_permissions|.
  bool IsAPIPrivilegeIncrease(const PermissionSet* old_permissions,
                              const PermissionSet* new_permissions,
                              Manifest::Type extension_type) const;

  // Returns true if |new_permissions| has an elevated manifest permission
  // privilege level compared to |old_permissions|.
  bool IsManifestPermissionPrivilegeIncrease(
      const PermissionSet* old_permissions,
      const PermissionSet* new_permissions) const;

  // Returns true if |new_permissions| has more host permissions compared to
  // |old_permissions|.
  bool IsHostPrivilegeIncrease(
      const PermissionSet* old_permissions,
      const PermissionSet* new_permissions,
      Manifest::Type extension_type) const;

  DISALLOW_COPY_AND_ASSIGN(ChromePermissionMessageProvider);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_CHROME_PERMISSION_MESSAGE_PROVIDER_H_
