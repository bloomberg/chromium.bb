// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_notification_manager.h"

namespace content {

LayoutTestNotificationManager::LayoutTestNotificationManager() {}

LayoutTestNotificationManager::~LayoutTestNotificationManager() {}

blink::WebNotificationPermission
LayoutTestNotificationManager::CheckPermission(const GURL& origin) {
  NotificationPermissionMap::iterator iter =
      permission_map_.find(origin);
  if (iter == permission_map_.end())
    return blink::WebNotificationPermissionDefault;

  return iter->second;
}

void LayoutTestNotificationManager::RequestPermission(
    const GURL& origin,
    const base::Callback<void(blink::WebNotificationPermission)>& callback) {
  callback.Run(CheckPermission(origin));
}

void LayoutTestNotificationManager::SetPermission(
    const GURL& origin,
    blink::WebNotificationPermission permission) {
  permission_map_[origin] = permission;
}

void LayoutTestNotificationManager::ClearPermissions() {
  permission_map_.clear();
}

}  // namespace content
