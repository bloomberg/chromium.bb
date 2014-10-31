// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/notification_provider.h"

#include <vector>

#include "base/strings/string_util.h"
#include "content/common/desktop_notification_messages.h"
#include "content/common/frame_messages.h"
#include "content/renderer/notification_icon_loader.h"
#include "content/renderer/render_frame_impl.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

using blink::WebDocument;
using blink::WebNotification;
using blink::WebNotificationPresenter;
using blink::WebSecurityOrigin;
using blink::WebString;

namespace content {

NotificationProvider::NotificationProvider(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {}

NotificationProvider::~NotificationProvider() {}

bool NotificationProvider::show(const WebNotification& notification) {
  int notification_id = manager_.RegisterNotification(notification);
  if (notification.iconURL().isEmpty()) {
    DisplayNotification(notification_id, SkBitmap());
    return true;
  }

  scoped_ptr<NotificationIconLoader> loader(
      new NotificationIconLoader(
          notification_id,
          base::Bind(&NotificationProvider::DisplayNotification,
                     base::Unretained(this))));

  loader->Start(notification.iconURL());

  pending_notifications_.push_back(loader.release());
  return true;
}

void NotificationProvider::DisplayNotification(int notification_id,
                                               const SkBitmap& icon) {
  WebDocument document = render_frame()->GetWebFrame()->document();
  WebNotification notification;

  if (!manager_.GetNotification(notification_id, &notification)) {
    NOTREACHED();
    return;
  }

  RemovePendingNotification(notification_id);

  ShowDesktopNotificationHostMsgParams params;
  params.origin = GURL(document.securityOrigin().toString());
  params.icon = icon;
  params.title = notification.title();
  params.body = notification.body();
  params.direction = notification.direction();
  params.replace_id = notification.replaceId();

  Send(new DesktopNotificationHostMsg_Show(routing_id(),
                                           notification_id,
                                           params));
}

bool NotificationProvider::RemovePendingNotification(int notification_id) {
  PendingNotifications::iterator iter = pending_notifications_.begin();
  for (; iter != pending_notifications_.end(); ++iter) {
    if ((*iter)->notification_id() != notification_id)
      continue;

    pending_notifications_.erase(iter);
    return true;
  }

  return false;
}

void NotificationProvider::cancel(const WebNotification& notification) {
  int id;
  bool id_found = manager_.GetId(notification, id);
  // Won't be found if the notification has already been closed by the user,
  // or if the notification's icon is still being requested.
  if (id_found && !RemovePendingNotification(id))
    Send(new DesktopNotificationHostMsg_Cancel(routing_id(), id));
}

void NotificationProvider::objectDestroyed(
    const WebNotification& notification) {
  int id;
  bool id_found = manager_.GetId(notification, id);
  // Won't be found if the notification has already been closed by the user.
  if (id_found) {
    RemovePendingNotification(id);
    manager_.UnregisterNotification(id);
  }
}

WebNotificationPresenter::Permission NotificationProvider::checkPermission(
    const WebSecurityOrigin& origin) {
  int permission = WebNotificationPresenter::PermissionNotAllowed;
  Send(new DesktopNotificationHostMsg_CheckPermission(
          routing_id(),
          GURL(origin.toString()),
          &permission));
  return static_cast<WebNotificationPresenter::Permission>(permission);
}

bool NotificationProvider::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NotificationProvider, message)
    IPC_MESSAGE_HANDLER(DesktopNotificationMsg_PostDisplay, OnDisplay);
    IPC_MESSAGE_HANDLER(DesktopNotificationMsg_PostClose, OnClose);
    IPC_MESSAGE_HANDLER(DesktopNotificationMsg_PostClick, OnClick);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (message.type() == FrameMsg_Navigate::ID)
    OnNavigate();  // Don't want to swallow the message.

  return handled;
}

void NotificationProvider::OnDisplay(int id) {
  WebNotification notification;
  bool found = manager_.GetNotification(id, &notification);
  // |found| may be false if the WebNotification went out of scope in
  // the page before it was actually displayed to the user.
  if (found)
    notification.dispatchDisplayEvent();
}

void NotificationProvider::OnClose(int id, bool by_user) {
  WebNotification notification;
  bool found = manager_.GetNotification(id, &notification);
  // |found| may be false if the WebNotification went out of scope in
  // the page before the associated toast was closed by the user.
  if (found) {
    notification.dispatchCloseEvent(by_user);
    manager_.UnregisterNotification(id);
  }
}

void NotificationProvider::OnClick(int id) {
  WebNotification notification;
  bool found = manager_.GetNotification(id, &notification);
  // |found| may be false if the WebNotification went out of scope in
  // the page before the associated toast was clicked on.
  if (found)
    notification.dispatchClickEvent();
}

void NotificationProvider::OnNavigate() {
  manager_.Clear();
}

}  // namespace content
