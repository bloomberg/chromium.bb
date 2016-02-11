// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_NOTIFICATIONS_PENDING_NOTIFICATION_H_
#define CONTENT_CHILD_NOTIFICATIONS_PENDING_NOTIFICATION_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/child/notifications/notification_image_loader.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/modules/notifications/WebNotificationData.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

struct NotificationResources;

// Stores the information associated with a pending notification, and fetches
// resources for it on the main thread.
class PendingNotification {
 public:
  explicit PendingNotification(
      const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner);
  ~PendingNotification();

  // Fetches all resources asynchronously on the main thread.
  void FetchResources(const blink::WebNotificationData& notification_data,
                      const base::Closure& fetches_finished_callback);

  // Returns a new NotificationResources populated with the resources that have
  // been fetched.
  NotificationResources GetResources() const;

 private:
  // Fetches an image using |image_web_url| asynchronously on the main thread.
  // The |image_callback| will be called on the worker thread.
  void FetchImageResource(const blink::WebURL& image_web_url,
                          const ImageLoadCompletedCallback& image_callback);

  // To be called on the worker thread when the notification icon has been
  // fetched.
  void DidFetchNotificationIcon(const SkBitmap& notification_icon);

  // To be called on the worker thread when an action icon has been fetched.
  void DidFetchActionIcon(size_t action_index, const SkBitmap& action_icon);

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  SkBitmap notification_icon_;

  std::vector<SkBitmap> action_icons_;

  base::Closure fetches_finished_barrier_closure_;

  std::vector<scoped_refptr<NotificationImageLoader>> image_loaders_;

  base::WeakPtrFactory<PendingNotification> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PendingNotification);
};

}  // namespace content

#endif  // CONTENT_CHILD_NOTIFICATIONS_PENDING_NOTIFICATION_H_
