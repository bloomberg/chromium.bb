// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/apps/platform_apps/chrome_apps_api_permissions.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "extensions/common/permissions/api_permission.h"

namespace apps {

ChromeAppsAPIPermissions::ChromeAppsAPIPermissions() = default;
ChromeAppsAPIPermissions::~ChromeAppsAPIPermissions() = default;

std::vector<std::unique_ptr<extensions::APIPermissionInfo>>
ChromeAppsAPIPermissions::GetAllPermissions() const {
  // WARNING: If you are modifying a permission message in this list, be sure to
  // add the corresponding permission message rule to
  // ChromePermissionMessageProvider::GetPermissionMessages as well.
  static constexpr extensions::APIPermissionInfo::InitInfo
      permissions_to_register[] = {
          {extensions::APIPermission::kBrowser, "browser"},
      };

  std::vector<std::unique_ptr<extensions::APIPermissionInfo>> permissions;
  permissions.reserve(base::size(permissions_to_register));

  for (const auto& permission : permissions_to_register) {
    // NOTE: Using base::WrapUnique() because APIPermissionsInfo ctor is
    // private.
    permissions.push_back(
        base::WrapUnique(new extensions::APIPermissionInfo(permission)));
  }

  return permissions;
}

}  // namespace apps
