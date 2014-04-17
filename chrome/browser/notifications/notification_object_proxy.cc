// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_object_proxy.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "content/public/browser/render_view_host.h"

using content::RenderViewHost;

NotificationObjectProxy::NotificationObjectProxy(
    int process_id, int route_id, int notification_id)
    : process_id_(process_id),
      route_id_(route_id),
      notification_id_(notification_id),
      displayed_(false) {
}

void NotificationObjectProxy::Display() {
  // This method is called each time the notification is shown to the user
  // but we only want to fire the event the first time.
  if (displayed_)
    return;
  displayed_ = true;

  RenderViewHost* host = RenderViewHost::FromID(process_id_, route_id_);
  if (host)
    host->DesktopNotificationPostDisplay(notification_id_);
}

void NotificationObjectProxy::Error() {
  RenderViewHost* host = RenderViewHost::FromID(process_id_, route_id_);
  if (host)
    host->DesktopNotificationPostError(notification_id_, base::string16());
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
  return base::StringPrintf("%d:%d:%d", process_id_, route_id_,
                            notification_id_);
}

int NotificationObjectProxy::process_id() const {
  return process_id_;
}

content::WebContents* NotificationObjectProxy::GetWebContents() const {
  return tab_util::GetWebContentsByID(process_id_, route_id_);
}
