// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_permission_manager.h"
#include "content/public/browser/permission_controller.h"

#include "third_party/blink/public/platform/modules/permissions/permission_status.mojom.h"

namespace content {

MockPermissionManager::MockPermissionManager() {}

MockPermissionManager::~MockPermissionManager() {}

int MockPermissionManager::RequestPermission(
    PermissionType permission,
    RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    const base::Callback<void(blink::mojom::PermissionStatus)>& callback) {
  return PermissionController::kNoPendingOperation;
}

int MockPermissionManager::RequestPermissions(
    const std::vector<PermissionType>& permission,
    RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    const base::Callback<
        void(const std::vector<blink::mojom::PermissionStatus>&)>& callback) {
  return PermissionController::kNoPendingOperation;
}

int MockPermissionManager::SubscribePermissionStatusChange(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const base::Callback<void(blink::mojom::PermissionStatus)>& callback) {
  // Return a fake subscription_id.
  return 0;
}

}  // namespace content
