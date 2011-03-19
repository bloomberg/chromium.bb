// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_object_proxy.h"

#include "base/message_loop.h"
#include "base/string16.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/common/desktop_notification_messages.h"

NotificationObjectProxy::NotificationObjectProxy(int process_id, int route_id,
    int notification_id, bool worker)
    : process_id_(process_id),
      route_id_(route_id),
      notification_id_(notification_id),
      worker_(worker) {
}

void NotificationObjectProxy::Display() {
  Send(new DesktopNotificationMsg_PostDisplay(route_id_, notification_id_));
}

void NotificationObjectProxy::Error() {
  Send(new DesktopNotificationMsg_PostError(
      route_id_, notification_id_, string16()));
}

void NotificationObjectProxy::Close(bool by_user) {
  Send(new DesktopNotificationMsg_PostClose(
      route_id_, notification_id_, by_user));
}

void NotificationObjectProxy::Click() {
  Send(new DesktopNotificationMsg_PostClick(route_id_, notification_id_));
}

std::string NotificationObjectProxy::id() const {
  return StringPrintf("%d:%d:%d:%d", process_id_, route_id_,
                      notification_id_, worker_);
}


void NotificationObjectProxy::Send(IPC::Message* message) {
  if (worker_) {
    // TODO(johnnyg): http://crbug.com/23065  Worker support coming soon.
    NOTREACHED();
    return;
  }

  RenderViewHost* host = RenderViewHost::FromID(process_id_, route_id_);
  if (host) {
    host->Send(message);
  } else {
    delete message;
  }
}
