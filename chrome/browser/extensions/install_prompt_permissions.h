// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_INSTALL_PROMPT_PERMISSIONS_H_
#define CHROME_BROWSER_EXTENSIONS_INSTALL_PROMPT_PERMISSIONS_H_

#include <vector>

#include "base/strings/string16.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/permission_message.h"

namespace extensions {

class PermissionSet;

struct InstallPromptPermissions {
  InstallPromptPermissions();
  ~InstallPromptPermissions();

  void LoadFromPermissionSet(const extensions::PermissionSet* permissions_set,
                             extensions::Manifest::Type type);

  void AddPermissionMessages(
      const extensions::PermissionMessages& permissions_messages);

  std::vector<base::string16> permissions;
  std::vector<base::string16> details;
  std::vector<bool> is_showing_details;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_INSTALL_PROMPT_PERMISSIONS_H_
