// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_NOTIFICATION_PROVIDER_H_
#define CONTENT_RENDERER_NOTIFICATION_PROVIDER_H_

#include "base/memory/scoped_vector.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/renderer/active_notification_tracker.h"
#include "third_party/WebKit/public/web/WebNotification.h"
#include "third_party/WebKit/public/web/WebNotificationPresenter.h"

class SkBitmap;

namespace content {

class NotificationIconLoader;

// NotificationProvider class is owned by the RenderFrame.  Only to be used on
// the main thread.
class NotificationProvider : public RenderFrameObserver,
                             public blink::WebNotificationPresenter {
 public:
  explicit NotificationProvider(RenderFrame* render_frame);
  ~NotificationProvider() override;

 private:
  // RenderFrameObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  // blink::WebNotificationPresenter implementation.
  virtual bool show(const blink::WebNotification& proxy);
  virtual void cancel(const blink::WebNotification& proxy);
  virtual void objectDestroyed(const blink::WebNotification& proxy);
  virtual blink::WebNotificationPresenter::Permission checkPermission(
      const blink::WebSecurityOrigin& origin);

  // Sends an IPC to the browser process to display the notification identified
  // by |notification|, accompanied by the downloaded icon.
  void DisplayNotification(int notification_id, const SkBitmap& icon);

  // Removes the IconDownload object associated with |notification_id|, stopping
  // any in-progress download. Returns whether the notification was removed.
  bool RemovePendingNotification(int notification_id);

  // IPC handlers.
  void OnDisplay(int id);
  void OnClose(int id, bool by_user);
  void OnClick(int id);
  void OnNavigate();

  // A vector tracking notifications whose icon is still being downloaded.
  typedef ScopedVector<NotificationIconLoader> PendingNotifications;
  PendingNotifications pending_notifications_;

  // A tracker object which manages the active notifications and the IDs
  // that are used to refer to them over IPC.
  ActiveNotificationTracker manager_;

  DISALLOW_COPY_AND_ASSIGN(NotificationProvider);
};

}  // namespace content

#endif  // CONTENT_RENDERER_NOTIFICATION_PROVIDER_H_
