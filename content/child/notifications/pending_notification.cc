// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/notifications/pending_notification.h"

#include <algorithm>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/thread_task_runner_handle.h"
#include "content/common/notification_constants.h"
#include "content/public/common/notification_resources.h"
#include "skia/ext/image_operations.h"
#include "url/gurl.h"

namespace content {

namespace {

// Scales down |icon| to fit within |max_size_px| if its width or height is
// larger than |max_size_px| and returns the result. Otherwise does nothing and
// returns |icon| unchanged.
SkBitmap ScaleDownIfNeeded(const SkBitmap& icon, int max_size_px) {
  if (icon.width() > max_size_px || icon.height() > max_size_px) {
    SCOPED_UMA_HISTOGRAM_TIMER("Notifications.Icon.ScaleDownTime");
    return skia::ImageOperations::Resize(icon,
                                         skia::ImageOperations::RESIZE_BEST,
                                         std::min(icon.width(), max_size_px),
                                         std::min(icon.height(), max_size_px));
  }
  return icon;
}

}  // namespace

PendingNotification::PendingNotification(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner)
    : main_task_runner_(main_task_runner), weak_factory_(this) {}

PendingNotification::~PendingNotification() {}

void PendingNotification::FetchResources(
    const blink::WebNotificationData& notification_data,
    const base::Closure& fetches_finished_callback) {
  // TODO(mvanouwerkerk): Add a timeout mechanism: crbug.com/579137

  size_t num_actions = notification_data.actions.size();
  action_icons_.resize(num_actions);

  size_t num_closures = 2 /* notification icon and badge */ + num_actions;
  fetches_finished_barrier_closure_ =
      base::BarrierClosure(num_closures, fetches_finished_callback);

  FetchImageResource(notification_data.icon,
                     base::Bind(&PendingNotification::DidFetchNotificationIcon,
                                weak_factory_.GetWeakPtr()));
  FetchImageResource(notification_data.badge,
                     base::Bind(&PendingNotification::DidFetchBadge,
                                weak_factory_.GetWeakPtr()));
  for (size_t i = 0; i < num_actions; i++) {
    FetchImageResource(notification_data.actions[i].icon,
                       base::Bind(&PendingNotification::DidFetchActionIcon,
                                  weak_factory_.GetWeakPtr(), i));
  }
}

NotificationResources PendingNotification::GetResources() const {
  NotificationResources resources;
  resources.notification_icon = notification_icon_;
  resources.badge = badge_;
  resources.action_icons = action_icons_;
  return resources;
}

void PendingNotification::FetchImageResource(
    const blink::WebURL& image_web_url,
    const ImageLoadCompletedCallback& image_callback) {
  if (image_web_url.isEmpty()) {
    image_callback.Run(SkBitmap());
    return;
  }

  // Explicitly convert the WebURL to a GURL before passing it to a different
  // thread. This is important because WebURLs must not be passed between
  // threads, and per base::Bind() semantics conversion would otherwise be done
  // on the receiving thread.
  GURL image_gurl(image_web_url);

  scoped_refptr<NotificationImageLoader> image_loader(
      new NotificationImageLoader(image_callback,
                                  base::ThreadTaskRunnerHandle::Get(),
                                  main_task_runner_));
  image_loaders_.push_back(image_loader);
  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&NotificationImageLoader::StartOnMainThread,
                            image_loader, image_gurl));
}

void PendingNotification::DidFetchNotificationIcon(const SkBitmap& bitmap) {
  notification_icon_ =
      ScaleDownIfNeeded(bitmap, kPlatformNotificationMaxIconSizePx);
  fetches_finished_barrier_closure_.Run();
}

void PendingNotification::DidFetchBadge(const SkBitmap& bitmap) {
  badge_ = ScaleDownIfNeeded(bitmap, kPlatformNotificationMaxBadgeSizePx);
  fetches_finished_barrier_closure_.Run();
}

void PendingNotification::DidFetchActionIcon(size_t action_index,
                                             const SkBitmap& bitmap) {
  DCHECK_LT(action_index, action_icons_.size());

  action_icons_[action_index] =
      ScaleDownIfNeeded(bitmap, kPlatformNotificationMaxActionIconSizePx);
  fetches_finished_barrier_closure_.Run();
}

}  // namespace content
