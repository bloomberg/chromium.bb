// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_NOTIFICATION_PROVIDER_H_
#define CHROME_RENDERER_NOTIFICATION_PROVIDER_H_

#include <map>

#include "chrome/common/desktop_notifications/active_notification_tracker.h"
#include "ipc/ipc_channel.h"
#include "third_party/WebKit/WebKit/chromium/public/WebNotification.h"
#include "third_party/WebKit/WebKit/chromium/public/WebNotificationPresenter.h"

class RenderView;
namespace WebKit {
class WebNotificationPermissionCallback;
}

// NotificationProvider class is owned by the RenderView.  Only
// to be used on the UI thread.
class NotificationProvider : public WebKit::WebNotificationPresenter {
 public:
  explicit NotificationProvider(RenderView* view);
  ~NotificationProvider() {}

  // WebKit::WebNotificationPresenter interface.
  virtual bool show(const WebKit::WebNotification& proxy);
  virtual void cancel(const WebKit::WebNotification& proxy);
  virtual void objectDestroyed(const WebKit::WebNotification& proxy);
  virtual WebKit::WebNotificationPresenter::Permission checkPermission(
      const WebKit::WebURL& url, WebKit::WebDocument* document);
  virtual void requestPermission(const WebKit::WebString& origin,
      WebKit::WebNotificationPermissionCallback* callback);

  // IPC message handler called from RenderView.
  bool OnMessageReceived(const IPC::Message& message);

  // Called when the RenderView navigates.
  void OnNavigate();

 private:
  // Internal methods used to show notifications.
  bool ShowHTML(const WebKit::WebNotification& notification, int id);
  bool ShowText(const WebKit::WebNotification& notification, int id);

  // IPC handlers.
  void OnDisplay(int id);
  void OnError(int id, const WebKit::WebString& message);
  void OnClose(int id, bool by_user);
  void OnPermissionRequestComplete(int id);

  bool Send(IPC::Message* message);

  // Non-owned pointer to the RenderView object which created and owns
  // this object.
  RenderView* view_;

  // A tracker object which manages the active notifications and the IDs
  // that are used to refer to them over IPC.
  ActiveNotificationTracker manager_;

  DISALLOW_COPY_AND_ASSIGN(NotificationProvider);
};

#endif  // CHROME_RENDERER_NOTIFICATION_PROVIDER_H_
