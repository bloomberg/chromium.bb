// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/notification_provider.h"

#include "base/task.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "webkit/api/public/WebNotificationPermissionCallback.h"

using WebKit::WebNotification;
using WebKit::WebNotificationPresenter;
using WebKit::WebNotificationPermissionCallback;
using WebKit::WebString;

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
  DCHECK(id_found);
  if (id_found)
    Send(new ViewHostMsg_CancelDesktopNotification(view_->routing_id(), id));
}

void NotificationProvider::objectDestroyed(
    const WebNotification& notification) {
  int id;
  bool id_found = manager_.GetId(notification, id);
  DCHECK(id_found);
  if (id_found)
    manager_.UnregisterNotification(id);
}

WebNotificationPresenter::Permission NotificationProvider::checkPermission(
    const WebString& origin) {
  int permission;
  Send(new ViewHostMsg_CheckNotificationPermission(view_->routing_id(), origin,
                                                   &permission));
  return static_cast<WebNotificationPresenter::Permission>(permission);
}

void NotificationProvider::requestPermission(
    const WebString& origin, WebNotificationPermissionCallback* callback) {
  int id = manager_.RegisterPermissionRequest(callback);

  Send(new ViewHostMsg_RequestNotificationPermission(view_->routing_id(),
                                                     origin, id));
}

bool NotificationProvider::ShowHTML(const WebNotification& notification,
                                    int id) {
  DCHECK(notification.isHTML());
  return Send(new ViewHostMsg_ShowDesktopNotification(view_->routing_id(),
              GURL(view_->webview()->mainFrame()->url()),
              notification.url(), id));
}

bool NotificationProvider::ShowText(const WebNotification& notification,
                                    int id) {
  DCHECK(!notification.isHTML());
  return Send(new ViewHostMsg_ShowDesktopNotificationText(view_->routing_id(),
              GURL(view_->webview()->mainFrame()->url()),
              GURL(notification.icon()),
              notification.title(), notification.body(), id));
}

void NotificationProvider::OnDisplay(int id) {
  RenderProcess::current()->main_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &NotificationProvider::HandleOnDisplay, id));
}

void NotificationProvider::OnError(int id, const WebString& message) {
  RenderProcess::current()->main_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &NotificationProvider::HandleOnError,
                        id, message));
}

void NotificationProvider::OnClose(int id, bool by_user) {
  RenderProcess::current()->main_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &NotificationProvider::HandleOnClose,
                        id, by_user));
}

void NotificationProvider::OnPermissionRequestComplete(int id) {
  RenderProcess::current()->main_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this,
          &NotificationProvider::HandleOnPermissionRequestComplete, id));
}

void NotificationProvider::HandleOnDisplay(int id) {
  DCHECK(MessageLoop::current()->type() == MessageLoop::TYPE_UI);
  WebNotification notification;
  bool found = manager_.GetNotification(id, &notification);
  // |found| may be false if the WebNotification went out of scope in
  // the page before it was actually displayed to the user.
  if (found)
    notification.dispatchDisplayEvent();
}

void NotificationProvider::HandleOnError(int id, const WebString& message) {
  DCHECK(MessageLoop::current()->type() == MessageLoop::TYPE_UI);
  WebNotification notification;
  bool found = manager_.GetNotification(id, &notification);
  // |found| may be false if the WebNotification went out of scope in
  // the page before the error occurred.
  if (found)
    notification.dispatchErrorEvent(message);
}

void NotificationProvider::HandleOnClose(int id, bool by_user) {
  DCHECK(MessageLoop::current()->type() == MessageLoop::TYPE_UI);
  WebNotification notification;
  bool found = manager_.GetNotification(id, &notification);
  // |found| may be false if the WebNotification went out of scope in
  // the page before the associated toast was closed by the user.
  if (found)
    notification.dispatchCloseEvent(by_user);
  manager_.UnregisterNotification(id);
}

void NotificationProvider::HandleOnPermissionRequestComplete(int id) {
  DCHECK(MessageLoop::current()->type() == MessageLoop::TYPE_UI);
  WebNotificationPermissionCallback* callback = manager_.GetCallback(id);
  DCHECK(callback);
  callback->permissionRequestComplete();
  manager_.OnPermissionRequestComplete(id);
}

bool NotificationProvider::OnMessageReceived(const IPC::Message& message) {
  if (message.routing_id() != view_->routing_id())
    return false;

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

bool NotificationProvider::Send(IPC::Message* message) {
  return RenderThread::current()->Send(message);
}
