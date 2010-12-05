// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_object_proxy.h"

#include "base/message_loop.h"
#include "base/string16.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/common/render_messages.h"

NotificationObjectProxy::NotificationObjectProxy(int process_id, int route_id,
    int notification_id, bool worker)
    : process_id_(process_id),
      route_id_(route_id),
      notification_id_(notification_id),
      worker_(worker) {
}

void NotificationObjectProxy::Display() {
  if (worker_) {
    // TODO(johnnyg): http://crbug.com/23065  Worker support coming soon.
    NOTREACHED();
  } else {
    DeliverMessage(new ViewMsg_PostDisplayToNotificationObject(
        route_id_, notification_id_));
  }
}

void NotificationObjectProxy::Error() {
  if (worker_) {
    // TODO(johnnyg): http://crbug.com/23065  Worker support coming soon.
    NOTREACHED();
  } else {
    DeliverMessage(new ViewMsg_PostErrorToNotificationObject(
        route_id_, notification_id_, string16()));
  }
}

void NotificationObjectProxy::Close(bool by_user) {
  if (worker_) {
    // TODO(johnnyg): http://crbug.com/23065  Worker support coming soon.
    NOTREACHED();
  } else {
    DeliverMessage(new ViewMsg_PostCloseToNotificationObject(
        route_id_, notification_id_, by_user));
  }
}

void NotificationObjectProxy::Click() {
  if (worker_) {
    NOTREACHED();
  } else {
    DeliverMessage(new ViewMsg_PostClickToNotificationObject(
        route_id_, notification_id_));
  }
}

std::string NotificationObjectProxy::id() const {
  return StringPrintf("%d:%d:%d:%d", process_id_, route_id_,
                      notification_id_, worker_);
}


void NotificationObjectProxy::DeliverMessage(IPC::Message* message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &NotificationObjectProxy::Send, message));
}

// Deferred method which runs on the IO thread and sends a message to the
// proxied notification, routing it through the correct host in the browser.
void NotificationObjectProxy::Send(IPC::Message* message) {
  // Take ownership of the message; ownership will pass to a host if possible.
  scoped_ptr<IPC::Message> owned_message(message);

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  RenderViewHost* host = RenderViewHost::FromID(process_id_, route_id_);
  if (host) {
    // Pass ownership to the host.
    host->Send(owned_message.release());
  }
}
