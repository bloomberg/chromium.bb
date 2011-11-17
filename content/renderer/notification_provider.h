// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_NOTIFICATION_PROVIDER_H_
#define CONTENT_RENDERER_NOTIFICATION_PROVIDER_H_
#pragma once

#include "content/public/renderer/render_view_observer.h"
#include "content/renderer/active_notification_tracker.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNotification.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNotificationPresenter.h"

class RenderViewImpl;

namespace WebKit {
class WebNotificationPermissionCallback;
}

// NotificationProvider class is owned by the RenderView.  Only
// to be used on the main thread.
class NotificationProvider : public content::RenderViewObserver,
                             public WebKit::WebNotificationPresenter {
 public:
  explicit NotificationProvider(RenderViewImpl* render_view);
  virtual ~NotificationProvider();

 private:
  // RenderView::Observer implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // WebKit::WebNotificationPresenter interface.
  virtual bool show(const WebKit::WebNotification& proxy);
  virtual void cancel(const WebKit::WebNotification& proxy);
  virtual void objectDestroyed(const WebKit::WebNotification& proxy);
  virtual WebKit::WebNotificationPresenter::Permission checkPermission(
      const WebKit::WebSecurityOrigin& origin);
  virtual void requestPermission(const WebKit::WebSecurityOrigin& origin,
      WebKit::WebNotificationPermissionCallback* callback);

  // Internal methods used to show notifications.
  bool ShowHTML(const WebKit::WebNotification& notification, int id);
  bool ShowText(const WebKit::WebNotification& notification, int id);

  // IPC handlers.
  void OnDisplay(int id);
  void OnError(int id, const WebKit::WebString& message);
  void OnClose(int id, bool by_user);
  void OnClick(int id);
  void OnPermissionRequestComplete(int id);
  void OnNavigate();

  // A tracker object which manages the active notifications and the IDs
  // that are used to refer to them over IPC.
  ActiveNotificationTracker manager_;

  DISALLOW_COPY_AND_ASSIGN(NotificationProvider);
};

#endif  // CONTENT_RENDERER_NOTIFICATION_PROVIDER_H_
