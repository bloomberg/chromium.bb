// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_NOTIFICATION_PROVIDER_H_
#define CHROME_RENDERER_NOTIFICATION_PROVIDER_H_

#include <map>

#include "chrome/common/desktop_notifications/active_notification_tracker.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "webkit/api/public/WebNotification.h"
#include "webkit/api/public/WebNotificationPresenter.h"

class RenderView;
namespace WebKit {
class WebNotificationPermissionCallback;
}

class NotificationProvider : public WebKit::WebNotificationPresenter,
                             public IPC::ChannelProxy::MessageFilter {
 public:
  explicit NotificationProvider(RenderView* view);
  ~NotificationProvider() {}

  // WebKit::WebNotificationPresenter interface.  Called from WebKit
  // on the UI thread.
  virtual bool show(const WebKit::WebNotification& proxy);
  virtual void cancel(const WebKit::WebNotification& proxy);
  virtual void objectDestroyed(const WebKit::WebNotification& proxy);
  virtual WebKit::WebNotificationPresenter::Permission checkPermission(
      const WebKit::WebString& origin);
  virtual void requestPermission(const WebKit::WebString& origin,
      WebKit::WebNotificationPermissionCallback* callback);

private:
  // Internal methods used on the UI thread.
  bool ShowHTML(const WebKit::WebNotification& notification, int id);
  bool ShowText(const WebKit::WebNotification& notification, int id);

  // Callback methods invoked when events happen on active notifications.
  void OnDisplay(int id);
  void OnError(int id, const WebKit::WebString& message);
  void OnClose(int id, bool by_user);
  void OnPermissionRequestComplete(int id);

  // Internal versions of the IPC handlers which run on the UI thread.
  void HandleOnDisplay(int id);
  void HandleOnError(int id, const WebKit::WebString& message);
  void HandleOnClose(int id, bool by_user);
  void HandleOnPermissionRequestComplete(int id);

  // IPC::ChannelProxy::MessageFilter override
  virtual bool OnMessageReceived(const IPC::Message& message);

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
