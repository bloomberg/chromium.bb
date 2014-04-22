// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_object_proxy.h"

#include "base/guid.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/desktop_notification_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

NotificationObjectProxy::NotificationObjectProxy(
    content::RenderFrameHost* render_frame_host,
    content::DesktopNotificationDelegate* delegate)
    : render_process_id_(render_frame_host->GetProcess()->GetID()),
      render_frame_id_(render_frame_host->GetRoutingID()),
      delegate_(delegate),
      displayed_(false),
      id_(base::GenerateGUID()) {
}

void NotificationObjectProxy::Display() {
  // This method is called each time the notification is shown to the user
  // but we only want to fire the event the first time.
  if (displayed_)
    return;
  displayed_ = true;

  delegate_->NotificationDisplayed();
}

void NotificationObjectProxy::Error() {
  delegate_->NotificationError();
}

void NotificationObjectProxy::Close(bool by_user) {
  delegate_->NotificationClosed(by_user);
}

void NotificationObjectProxy::Click() {
  delegate_->NotificationClick();
}

std::string NotificationObjectProxy::id() const {
  return id_;
}

int NotificationObjectProxy::process_id() const {
  return render_process_id_;
}

content::WebContents* NotificationObjectProxy::GetWebContents() const {
  return content::WebContents::FromRenderFrameHost(
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_));
}
