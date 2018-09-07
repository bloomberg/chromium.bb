// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_APPS_PLATFORM_APPS_CHROME_APPS_API_PERMISSIONS_H_
#define CHROME_COMMON_APPS_PLATFORM_APPS_CHROME_APPS_API_PERMISSIONS_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "extensions/common/permissions/permissions_provider.h"

namespace apps {

// A PermissionsProvider responsible for Chrome App API Permissions.
class ChromeAppsAPIPermissions : public extensions::PermissionsProvider {
 public:
  ChromeAppsAPIPermissions();
  ~ChromeAppsAPIPermissions() override;

  // extensions::PermissionsProvider:
  std::vector<std::unique_ptr<extensions::APIPermissionInfo>>
  GetAllPermissions() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeAppsAPIPermissions);
};

}  // namespace apps

#endif  // CHROME_COMMON_APPS_PLATFORM_APPS_CHROME_APPS_API_PERMISSIONS_H_
