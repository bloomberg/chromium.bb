// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/apps/platform_apps/chrome_apps_api_permissions.h"

namespace chrome_apps_api_permissions {
namespace {

// WARNING: If you are modifying a permission message in this list, be sure to
// add the corresponding permission message rule to
// ChromePermissionMessageProvider::GetPermissionMessages as well.
constexpr extensions::APIPermissionInfo::InitInfo permissions_to_register[] = {
    {extensions::APIPermission::kBrowser, "browser"},
    {extensions::APIPermission::kEasyUnlockPrivate, "easyUnlockPrivate"},
};

}  // namespace

base::span<const extensions::APIPermissionInfo::InitInfo> GetPermissionInfos() {
  return base::make_span(permissions_to_register);
}

}  // namespace chrome_apps_api_permissions
