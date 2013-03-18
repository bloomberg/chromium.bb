// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_object_proxy.h"

#include "base/stringprintf.h"
#include "content/public/browser/render_view_host.h"

using content::RenderViewHost;

NotificationObjectProxy::NotificationObjectProxy(int process_id, int route_id,
    int notification_id, bool worker)
    : process_id_(process_id),
      route_id_(route_id),
      notification_id_(notification_id),
      worker_(worker) {
  if (worker_) {
    // TODO(johnnyg): http://crbug.com/23065  Worker support coming soon.
    NOTREACHED();
  }
}

void NotificationObjectProxy::Display() {
  RenderViewHost* host = RenderViewHost::FromID(process_id_, route_id_);
  if (host)
    host->DesktopNotificationPostDisplay(notification_id_);
}

void NotificationObjectProxy::Error() {
  RenderViewHost* host = RenderViewHost::FromID(process_id_, route_id_);
  if (host)
    host->DesktopNotificationPostError(notification_id_, string16());
}

void NotificationObjectProxy::Close(bool by_user) {
  RenderViewHost* host = RenderViewHost::FromID(process_id_, route_id_);
  if (host)
    host->DesktopNotificationPostClose(notification_id_, by_user);
}

void NotificationObjectProxy::Click() {
  RenderViewHost* host = RenderViewHost::FromID(process_id_, route_id_);
  if (host)
    host->DesktopNotificationPostClick(notification_id_);
}

std::string NotificationObjectProxy::id() const {
  return base::StringPrintf("%d:%d:%d:%d", process_id_, route_id_,
                            notification_id_, worker_);
}

RenderViewHost* NotificationObjectProxy::GetRenderViewHost() const {
  return RenderViewHost::FromID(process_id_, route_id_);
}
