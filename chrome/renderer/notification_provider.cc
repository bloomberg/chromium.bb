// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/notification_provider.h"

#include "base/string_util.h"
#include "base/task.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebNotificationPermissionCallback.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"

using WebKit::WebDocument;
using WebKit::WebNotification;
using WebKit::WebNotificationPresenter;
using WebKit::WebNotificationPermissionCallback;
using WebKit::WebString;
using WebKit::WebURL;

NotificationProvider::NotificationProvider(RenderView* view)
    : view_(view) {
}

bool NotificationProvider::show(const WebNotification& notification) {
  int notification_id = manager_.RegisterNotification(notification);
  if (notification.isHTML())
    return ShowHTML(notification, notification_id);
  else
    return ShowText(notification, notification_id);
}

void NotificationProvider::cancel(const WebNotification& notification) {
  int id;
  bool id_found = manager_.GetId(notification, id);
  // Won't be found if the notification has already been closed by the user.
  if (id_found)
    Send(new ViewHostMsg_CancelDesktopNotification(view_->routing_id(), id));
}

void NotificationProvider::objectDestroyed(
    const WebNotification& notification) {
  int id;
  bool id_found = manager_.GetId(notification, id);
  // Won't be found if the notification has already been closed by the user.
  if (id_found)
    manager_.UnregisterNotification(id);
}

WebNotificationPresenter::Permission NotificationProvider::checkPermission(
    const WebURL& url, WebDocument* document) {
  int permission;
  Send(new ViewHostMsg_CheckNotificationPermission(
          view_->routing_id(),
          url,
          document ? UTF16ToASCII(document->applicationID()) : "",
          &permission));
  return static_cast<WebNotificationPresenter::Permission>(permission);
}

void NotificationProvider::requestPermission(
    const WebString& origin, WebNotificationPermissionCallback* callback) {
  // We only request permission in response to a user gesture.
  if (!view_->webview()->mainFrame()->isProcessingUserGesture())
    return;

  int id = manager_.RegisterPermissionRequest(callback);

  Send(new ViewHostMsg_RequestNotificationPermission(view_->routing_id(),
                                                     GURL(origin), id));
}

bool NotificationProvider::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NotificationProvider, message)
    IPC_MESSAGE_HANDLER(ViewMsg_PostDisplayToNotificationObject, OnDisplay);
    IPC_MESSAGE_HANDLER(ViewMsg_PostErrorToNotificationObject, OnError);
    IPC_MESSAGE_HANDLER(ViewMsg_PostCloseToNotificationObject, OnClose);
    IPC_MESSAGE_HANDLER(ViewMsg_PermissionRequestDone,
                        OnPermissionRequestComplete);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void NotificationProvider::OnNavigate() {
  //  manager_.Clear();
}

bool NotificationProvider::ShowHTML(const WebNotification& notification,
                                    int id) {
  // Disallow HTML notifications from non-HTTP schemes.
  GURL url = notification.url();
  if (!url.SchemeIs(chrome::kHttpScheme) &&
      !url.SchemeIs(chrome::kHttpsScheme) &&
      !url.SchemeIs(chrome::kExtensionScheme))
    return false;

  DCHECK(notification.isHTML());
  return Send(new ViewHostMsg_ShowDesktopNotification(view_->routing_id(),
              GURL(view_->webview()->mainFrame()->url()).GetOrigin(),
              notification.url(), id));
}

bool NotificationProvider::ShowText(const WebNotification& notification,
                                    int id) {
  DCHECK(!notification.isHTML());
  return Send(new ViewHostMsg_ShowDesktopNotificationText(view_->routing_id(),
              GURL(view_->webview()->mainFrame()->url()).GetOrigin(),
              GURL(notification.icon()),
              notification.title(), notification.body(), id));
}

void NotificationProvider::OnDisplay(int id) {
  WebNotification notification;
  bool found = manager_.GetNotification(id, &notification);
  // |found| may be false if the WebNotification went out of scope in
  // the page before it was actually displayed to the user.
  if (found)
    notification.dispatchDisplayEvent();
}

void NotificationProvider::OnError(int id, const WebString& message) {
  WebNotification notification;
  bool found = manager_.GetNotification(id, &notification);
  // |found| may be false if the WebNotification went out of scope in
  // the page before the error occurred.
  if (found)
    notification.dispatchErrorEvent(message);
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

void NotificationProvider::OnPermissionRequestComplete(int id) {
  WebNotificationPermissionCallback* callback = manager_.GetCallback(id);
  DCHECK(callback);
  callback->permissionRequestComplete();
  manager_.OnPermissionRequestComplete(id);
}

bool NotificationProvider::Send(IPC::Message* message) {
  return RenderThread::current()->Send(message);
}
