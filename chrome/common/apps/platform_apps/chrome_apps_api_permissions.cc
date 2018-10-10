// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/apps/platform_apps/chrome_apps_api_permissions.h"

#include "chrome/common/apps/platform_apps/media_galleries_permission.h"

namespace chrome_apps_api_permissions {
namespace {

template <typename T>
extensions::APIPermission* CreateAPIPermission(
    const extensions::APIPermissionInfo* permission) {
  return new T(permission);
}

// WARNING: If you are modifying a permission message in this list, be sure to
// add the corresponding permission message rule to
// ChromePermissionMessageProvider::GetPermissionMessages as well.
constexpr extensions::APIPermissionInfo::InitInfo permissions_to_register[] = {
    {extensions::APIPermission::kBrowser, "browser"},
    {extensions::APIPermission::kEasyUnlockPrivate, "easyUnlockPrivate"},
    {extensions::APIPermission::kMusicManagerPrivate, "musicManagerPrivate",
     extensions::APIPermissionInfo::kFlagCannotBeOptional},
    {extensions::APIPermission::kMediaGalleries, "mediaGalleries",
     extensions::APIPermissionInfo::kFlagNone,
     &CreateAPIPermission<chrome_apps::MediaGalleriesPermission>},
    {extensions::APIPermission::kSyncFileSystem, "syncFileSystem"},
};

}  // namespace

base::span<const extensions::APIPermissionInfo::InitInfo> GetPermissionInfos() {
  return base::make_span(permissions_to_register);
}

}  // namespace chrome_apps_api_permissions
