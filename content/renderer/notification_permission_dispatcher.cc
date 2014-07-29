// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/notification_permission_dispatcher.h"

#include "content/common/platform_notification_messages.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebNotificationPermissionCallback.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "url/gurl.h"

namespace content {

NotificationPermissionDispatcher::NotificationPermissionDispatcher(
    RenderFrame* render_frame) : RenderFrameObserver(render_frame) {
}

NotificationPermissionDispatcher::~NotificationPermissionDispatcher() {
}

void NotificationPermissionDispatcher::RequestPermission(
    const blink::WebSecurityOrigin& origin,
    blink::WebNotificationPermissionCallback* callback) {
  int request_id = pending_requests_.Add(callback);
  Send(new PlatformNotificationHostMsg_RequestPermission(
      routing_id(), GURL(origin.toString()), request_id));
}

bool NotificationPermissionDispatcher::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NotificationPermissionDispatcher, message)
    IPC_MESSAGE_HANDLER(PlatformNotificationMsg_PermissionRequestComplete,
                        OnPermissionRequestComplete);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void NotificationPermissionDispatcher::OnPermissionRequestComplete(
    int request_id, blink::WebNotificationPermission result) {
  blink::WebNotificationPermissionCallback* callback =
      pending_requests_.Lookup(request_id);
  DCHECK(callback);

  callback->permissionRequestComplete(result);
  pending_requests_.Remove(request_id);
}

}  // namespace content
