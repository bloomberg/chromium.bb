// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/message_center_stats_collector.h"

#include <string>

#include "base/metrics/histogram.h"
#include "content/public/browser/user_metrics.h"
#include "ui/message_center/message_center.h"

MessageCenterStatsCollector::NotificationStats::NotificationStats() {}

MessageCenterStatsCollector::NotificationStats::NotificationStats(
    const std::string& id) : id_(id) {
  for (size_t i = 0; i < NOTIFICATION_ACTION_COUNT; i++) {
    actions_[i] = false;
  }
}

MessageCenterStatsCollector::NotificationStats::~NotificationStats() {}

void MessageCenterStatsCollector::NotificationStats::CollectAction(
    NotificationActionType type) {
  DCHECK(!id_.empty());

  UMA_HISTOGRAM_ENUMERATION("Notifications.Actions",
                            type,
                            NOTIFICATION_ACTION_COUNT);
  actions_[type] = true;
}

void MessageCenterStatsCollector::NotificationStats::RecordAggregateStats() {
  DCHECK(!id_.empty());

  for (size_t i = 0; i < NOTIFICATION_ACTION_COUNT; i++) {
    if (!actions_[i])
      continue;
    UMA_HISTOGRAM_ENUMERATION("Notifications.PerNotificationActions",
                              i,
                              NOTIFICATION_ACTION_COUNT);
  }
}

MessageCenterStatsCollector::MessageCenterStatsCollector(
    message_center::MessageCenter* message_center)
    : message_center_(message_center) {
  message_center_->AddObserver(this);
}

MessageCenterStatsCollector::~MessageCenterStatsCollector() {
  message_center_->RemoveObserver(this);
}

void MessageCenterStatsCollector::OnNotificationAdded(
    const std::string& notification_id) {
  stats_[notification_id] = NotificationStats(notification_id);

  StatsCollection::iterator iter = stats_.find(notification_id);
  DCHECK(iter != stats_.end());

  stats_[notification_id].CollectAction(NOTIFICATION_ACTION_ADD);
}

void MessageCenterStatsCollector::OnNotificationRemoved(
    const std::string& notification_id, bool by_user) {
  StatsCollection::iterator iter = stats_.find(notification_id);
  if (iter == stats_.end())
    return;
  NotificationStats& notification_stat = iter->second;
  notification_stat.CollectAction(by_user ?
      NOTIFICATION_ACTION_CLOSE_BY_USER :
      NOTIFICATION_ACTION_CLOSE_BY_SYSTEM);
  notification_stat.RecordAggregateStats();
  stats_.erase(notification_id);
}

void MessageCenterStatsCollector::OnNotificationUpdated(
    const std::string& notification_id) {
  StatsCollection::iterator iter = stats_.find(notification_id);
  if (iter == stats_.end())
    return;
  NotificationStats& notification_stat = iter->second;

  notification_stat.CollectAction(NOTIFICATION_ACTION_UPDATE);
}

void MessageCenterStatsCollector::OnNotificationClicked(
    const std::string& notification_id) {
  StatsCollection::iterator iter = stats_.find(notification_id);
  if (iter == stats_.end())
    return;
  NotificationStats& notification_stat = iter->second;

  notification_stat.CollectAction(NOTIFICATION_ACTION_CLICK);
}

void MessageCenterStatsCollector::OnNotificationButtonClicked(
    const std::string& notification_id,
                                           int button_index) {
  StatsCollection::iterator iter = stats_.find(notification_id);
  if (iter == stats_.end())
    return;
  NotificationStats& notification_stat = iter->second;

  notification_stat.CollectAction(NOTIFICATION_ACTION_BUTTON_CLICK);
}

void MessageCenterStatsCollector::OnNotificationDisplayed(
    const std::string& notification_id,
    const message_center::DisplaySource source) {
  StatsCollection::iterator iter = stats_.find(notification_id);
  if (iter == stats_.end())
    return;
  NotificationStats& notification_stat = iter->second;

  notification_stat.CollectAction(NOTIFICATION_ACTION_DISPLAY);
}

void MessageCenterStatsCollector::OnCenterVisibilityChanged(
    message_center::Visibility visibility) {
  switch (visibility) {
    case message_center::VISIBILITY_TRANSIENT:
      break;
    case message_center::VISIBILITY_MESSAGE_CENTER:
      content::RecordAction(
          base::UserMetricsAction("Notifications.ShowMessageCenter"));
      break;
    case message_center::VISIBILITY_SETTINGS:
      content::RecordAction(
          base::UserMetricsAction("Notifications.ShowSettings"));
      break;
  }
}

void MessageCenterStatsCollector::OnQuietModeChanged(bool in_quiet_mode) {
  if (in_quiet_mode) {
    content::RecordAction(
        base::UserMetricsAction("Notifications.Mute"));
  } else {
    content::RecordAction(
        base::UserMetricsAction("Notifications.Unmute"));
  }
}

