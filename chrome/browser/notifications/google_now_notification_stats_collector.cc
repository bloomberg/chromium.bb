// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/google_now_notification_stats_collector.h"

#include <string>

#include "base/metrics/sparse_histogram.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/extension_welcome_notification.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "content/public/browser/user_metrics.h"
#include "ui/message_center/notification.h"

namespace {
const int kNotificationsMaxCount = 20;
}

GoogleNowNotificationStatsCollector::GoogleNowNotificationStatsCollector(
    message_center::MessageCenter* message_center)
    : message_center_(message_center) {
  message_center_->AddObserver(this);
}

GoogleNowNotificationStatsCollector::~GoogleNowNotificationStatsCollector() {
  message_center_->RemoveObserver(this);
}

void GoogleNowNotificationStatsCollector::OnNotificationDisplayed(
    const std::string& notification_id,
    const message_center::DisplaySource source) {
  if ((source == message_center::DISPLAY_SOURCE_POPUP) &&
      IsNotificationIdForGoogleNow(notification_id)) {
    content::RecordAction(
        base::UserMetricsAction(
            "GoogleNow.MessageCenter.NotificationPoppedUp"));
  }
}

void GoogleNowNotificationStatsCollector::OnCenterVisibilityChanged(
    message_center::Visibility visibility) {
  if (visibility == message_center::VISIBILITY_MESSAGE_CENTER) {
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "GoogleNow.MessageCenter.Displayed.NotificationsVisible",
        std::min(CountVisibleGoogleNowNotifications(), kNotificationsMaxCount));
  }
}

bool GoogleNowNotificationStatsCollector::IsNotificationIdForGoogleNow(
  const std::string& notification_id) {
  bool isGoogleNowNotification = false;
  const Notification* const notification =
      g_browser_process->notification_ui_manager()->FindById(notification_id);
  if (notification) {
    isGoogleNowNotification =
        ((notification->notifier_id().type ==
              message_center::NotifierId::APPLICATION) &&
        (notification->notifier_id().id ==
              ExtensionWelcomeNotification::kChromeNowExtensionID));
  }
  return isGoogleNowNotification;
}

int GoogleNowNotificationStatsCollector::CountVisibleGoogleNowNotifications() {
  typedef message_center::NotificationList::Notifications Notifications;
  const Notifications visible_notifications =
      message_center_->GetVisibleNotifications();
  int google_now_notification_count = 0;
  for (Notifications::iterator iter = visible_notifications.begin();
      iter != visible_notifications.end();
      ++iter) {
    if ((*iter)->notifier_id().id ==
            ExtensionWelcomeNotification::kChromeNowExtensionID)
      google_now_notification_count++;
  }
  return google_now_notification_count;
}
