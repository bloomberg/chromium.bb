// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_permission_manager.h"

#include "base/callback.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_type.h"

namespace headless {

HeadlessPermissionManager::HeadlessPermissionManager(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

HeadlessPermissionManager::~HeadlessPermissionManager() = default;

int HeadlessPermissionManager::RequestPermission(
    content::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    const base::Callback<void(blink::mojom::PermissionStatus)>& callback) {
  // In headless mode we just pretent the user "closes" any permission prompt,
  // without accepting or denying. Notifications are the exception to this,
  // which are explicitly disabled in Incognito mode.
  if (browser_context_->IsOffTheRecord() &&
      permission == content::PermissionType::NOTIFICATIONS) {
    callback.Run(blink::mojom::PermissionStatus::DENIED);
    return kNoPendingOperation;
  }

  callback.Run(blink::mojom::PermissionStatus::ASK);
  return kNoPendingOperation;
}

int HeadlessPermissionManager::RequestPermissions(
    const std::vector<content::PermissionType>& permissions,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    const base::Callback<
        void(const std::vector<blink::mojom::PermissionStatus>&)>& callback) {
  // In headless mode we just pretent the user "closes" any permission prompt,
  // without accepting or denying.
  std::vector<blink::mojom::PermissionStatus> result(
      permissions.size(), blink::mojom::PermissionStatus::ASK);
  callback.Run(result);
  return kNoPendingOperation;
}

void HeadlessPermissionManager::CancelPermissionRequest(int request_id) {}

void HeadlessPermissionManager::ResetPermission(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {}

blink::mojom::PermissionStatus HeadlessPermissionManager::GetPermissionStatus(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  return blink::mojom::PermissionStatus::ASK;
}

int HeadlessPermissionManager::SubscribePermissionStatusChange(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const base::Callback<void(blink::mojom::PermissionStatus)>& callback) {
  return kNoPendingOperation;
}

void HeadlessPermissionManager::UnsubscribePermissionStatusChange(
    int subscription_id) {}

}  // namespace headless
