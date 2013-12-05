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

class ChromePermissionMessageProvider : public PermissionMessageProvider {
 public:
  ChromePermissionMessageProvider();
  virtual ~ChromePermissionMessageProvider();

  // PermissionMessageProvider implementation.
  virtual PermissionMessages GetPermissionMessages(
      const PermissionSet* permissions,
      Manifest::Type extension_type) const OVERRIDE;
  virtual std::vector<base::string16> GetWarningMessages(
      const PermissionSet* permissions,
      Manifest::Type extension_type) const OVERRIDE;
  virtual std::vector<base::string16> GetWarningMessagesDetails(
      const PermissionSet* permissions,
      Manifest::Type extension_type) const OVERRIDE;
  virtual bool IsPrivilegeIncrease(
      const PermissionSet* old_permissions,
      const PermissionSet* new_permissions,
      Manifest::Type extension_type) const OVERRIDE;

 private:
  // Gets the permission messages for the API permissions.
  std::set<PermissionMessage> GetAPIPermissionMessages(
      const PermissionSet* permissions) const;

  // Gets the permission messages for the Manifest permissions.
  std::set<PermissionMessage> GetManifestPermissionMessages(
      const PermissionSet* permissions) const;

  // Gets the permission messages for the host permissions.
  std::set<PermissionMessage> GetHostPermissionMessages(
      const PermissionSet* permissions,
      Manifest::Type extension_type) const;

  // Returns true if |new_permissions| has an elevated API privilege level
  // compared to |old_permissions|.
  bool IsAPIPrivilegeIncrease(
      const PermissionSet* old_permissions,
      const PermissionSet* new_permissions) const;

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
