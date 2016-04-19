// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/notification_permission_dispatcher.h"

#include <utility>

#include "base/bind.h"
#include "content/public/common/service_registry.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission_status.mojom.h"
#include "third_party/WebKit/public/web/modules/notifications/WebNotificationPermissionCallback.h"

using blink::WebNotificationPermissionCallback;

namespace content {

NotificationPermissionDispatcher::NotificationPermissionDispatcher(
    RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {}

NotificationPermissionDispatcher::~NotificationPermissionDispatcher() {}

void NotificationPermissionDispatcher::RequestPermission(
    const blink::WebSecurityOrigin& origin,
    WebNotificationPermissionCallback* callback) {
  if (!permission_service_.get()) {
    render_frame()->GetServiceRegistry()->ConnectToRemoteService(
        mojo::GetProxy(&permission_service_));
  }

  std::unique_ptr<WebNotificationPermissionCallback> owned_callback(callback);

  // base::Unretained is safe here because the Mojo channel, with associated
  // callbacks, will be deleted before the "this" instance is deleted.
  permission_service_->RequestPermission(
      blink::mojom::PermissionName::NOTIFICATIONS, origin.toString().utf8(),
      base::Bind(&NotificationPermissionDispatcher::OnPermissionRequestComplete,
                 base::Unretained(this),
                 base::Passed(std::move(owned_callback))));
}

void NotificationPermissionDispatcher::OnPermissionRequestComplete(
    std::unique_ptr<WebNotificationPermissionCallback> callback,
    blink::mojom::PermissionStatus status) {
  DCHECK(callback);

  callback->permissionRequestComplete(status);
}

}  // namespace content
