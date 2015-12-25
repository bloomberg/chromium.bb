// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_NOTIFICATION_PERMISSION_DISPATCHER_H_
#define CONTENT_RENDERER_NOTIFICATION_PERMISSION_DISPATCHER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/permission_service.mojom.h"
#include "content/public/renderer/render_frame_observer.h"

namespace blink {
class WebNotificationPermissionCallback;
class WebSecurityOrigin;
}

namespace content {

class RenderFrame;

// Dispatcher for Web Notification permission requests.
class NotificationPermissionDispatcher : public RenderFrameObserver {
 public:
  explicit NotificationPermissionDispatcher(RenderFrame* render_frame);
  ~NotificationPermissionDispatcher() override;

  // Requests permission to display Web Notifications for |origin|. The callback
  // will be invoked when the permission status is available. This class will
  // take ownership of |callback|.
  void RequestPermission(const blink::WebSecurityOrigin& origin,
                         blink::WebNotificationPermissionCallback* callback);

 private:
  void OnPermissionRequestComplete(
      scoped_ptr<blink::WebNotificationPermissionCallback> callback,
      PermissionStatus status);

  PermissionServicePtr permission_service_;

  DISALLOW_COPY_AND_ASSIGN(NotificationPermissionDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_NOTIFICATION_PERMISSION_DISPATCHER_H_
