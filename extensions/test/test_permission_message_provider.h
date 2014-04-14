// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_TEST_TEST_PERMISSION_MESSAGE_PROVIDER_H_
#define EXTENSIONS_TEST_TEST_PERMISSION_MESSAGE_PROVIDER_H_

#include "base/macros.h"
#include "extensions/common/permissions/permission_message_provider.h"

namespace extensions {

class TestPermissionMessageProvider : public PermissionMessageProvider {
 public:
  TestPermissionMessageProvider();
  virtual ~TestPermissionMessageProvider();

 private:
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

  DISALLOW_COPY_AND_ASSIGN(TestPermissionMessageProvider);
};

}  // namespace extensions

#endif  // EXTENSIONS_TEST_TEST_PERMISSION_MESSAGE_PROVIDER_H_
