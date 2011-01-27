// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/notification_provider.h"

#include "base/string_util.h"
#include "base/task.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNotificationPermissionCallback.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using WebKit::WebDocument;
using WebKit::WebNotification;
using WebKit::WebNotificationPresenter;
using WebKit::WebNotificationPermissionCallback;
using WebKit::WebSecurityOrigin;
using WebKit::WebString;
using WebKit::WebURL;

NotificationProvider::NotificationProvider(RenderView* render_view)
    : RenderViewObserver(render_view) {
}

NotificationProvider::~NotificationProvider() {
  manager_.DetachAll();
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
    Send(new ViewHostMsg_CancelDesktopNotification(routing_id(), id));
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
    const WebURL& url) {
  int permission;
  Send(new ViewHostMsg_CheckNotificationPermission(
          routing_id(),
          url,
          &permission));
  return static_cast<WebNotificationPresenter::Permission>(permission);
}

void NotificationProvider::requestPermission(
    const WebSecurityOrigin& origin,
    WebNotificationPermissionCallback* callback) {
  // We only request permission in response to a user gesture.
  if (!render_view()->webview()->mainFrame()->isProcessingUserGesture())
    return;

  int id = manager_.RegisterPermissionRequest(callback);

  Send(new ViewHostMsg_RequestNotificationPermission(routing_id(),
                                                     GURL(origin.toString()),
                                                     id));
}

bool NotificationProvider::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NotificationProvider, message)
    IPC_MESSAGE_HANDLER(ViewMsg_PostDisplayToNotificationObject, OnDisplay);
    IPC_MESSAGE_HANDLER(ViewMsg_PostErrorToNotificationObject, OnError);
    IPC_MESSAGE_HANDLER(ViewMsg_PostCloseToNotificationObject, OnClose);
    IPC_MESSAGE_HANDLER(ViewMsg_PostClickToNotificationObject, OnClick);
    IPC_MESSAGE_HANDLER(ViewMsg_PermissionRequestDone,
                        OnPermissionRequestComplete);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (message.type() == ViewMsg_Navigate::ID)
    OnNavigate();  // Don't want to swallow the message.

  return handled;
}

bool NotificationProvider::ShowHTML(const WebNotification& notification,
                                    int id) {
  // Disallow HTML notifications from unwanted schemes.  javascript:
  // in particular allows unwanted cross-domain access.
  GURL url = notification.url();
  if (!url.SchemeIs(chrome::kHttpScheme) &&
      !url.SchemeIs(chrome::kHttpsScheme) &&
      !url.SchemeIs(chrome::kExtensionScheme) &&
      !url.SchemeIs(chrome::kDataScheme))
    return false;

  DCHECK(notification.isHTML());
  ViewHostMsg_ShowNotification_Params params;
  params.origin =
      GURL(render_view()->webview()->mainFrame()->url()).GetOrigin();
  params.is_html = true;
  params.contents_url = notification.url();
  params.notification_id = id;
  params.replace_id = notification.replaceId();
  return Send(new ViewHostMsg_ShowDesktopNotification(routing_id(), params));
}

bool NotificationProvider::ShowText(const WebNotification& notification,
                                    int id) {
  DCHECK(!notification.isHTML());
  ViewHostMsg_ShowNotification_Params params;
  params.is_html = false;
  params.origin = GURL(
      render_view()->webview()->mainFrame()->url()).GetOrigin();
  params.icon_url = notification.iconURL();
  params.title = notification.title();
  params.body = notification.body();
  params.direction = notification.direction();
  params.notification_id = id;
  params.replace_id = notification.replaceId();

  return Send(new ViewHostMsg_ShowDesktopNotification(routing_id(), params));
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

void NotificationProvider::OnClick(int id) {
  WebNotification notification;
  bool found = manager_.GetNotification(id, &notification);
  // |found| may be false if the WebNotification went out of scope in
  // the page before the associated toast was clicked on.
  if (found)
    notification.dispatchClickEvent();
}

void NotificationProvider::OnPermissionRequestComplete(int id) {
  WebNotificationPermissionCallback* callback = manager_.GetCallback(id);
  DCHECK(callback);
  callback->permissionRequestComplete();
  manager_.OnPermissionRequestComplete(id);
}

void NotificationProvider::OnNavigate() {
  manager_.Clear();
}
