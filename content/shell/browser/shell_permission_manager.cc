// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_permission_manager.h"

#include "base/callback.h"
#include "content/public/browser/permission_type.h"

namespace content {

ShellPermissionManager::ShellPermissionManager()
    : PermissionManager() {
}

ShellPermissionManager::~ShellPermissionManager() {
}

int ShellPermissionManager::RequestPermission(
    PermissionType permission,
    RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    const base::Callback<void(PermissionStatus)>& callback) {
  callback.Run(permission == PermissionType::GEOLOCATION
                   ? PERMISSION_STATUS_GRANTED : PERMISSION_STATUS_DENIED);
  return kNoPendingOperation;
}

void ShellPermissionManager::CancelPermissionRequest(
    PermissionType permission,
    RenderFrameHost* render_frame_host,
    int request_id,
    const GURL& requesting_origin) {
}

void ShellPermissionManager::ResetPermission(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
}

PermissionStatus ShellPermissionManager::GetPermissionStatus(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  return PERMISSION_STATUS_DENIED;
}

void ShellPermissionManager::RegisterPermissionUsage(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
}

int ShellPermissionManager::SubscribePermissionStatusChange(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const base::Callback<void(PermissionStatus)>& callback) {
  return kNoPendingOperation;
}

void ShellPermissionManager::UnsubscribePermissionStatusChange(
    int subscription_id) {
}

}  // namespace content
