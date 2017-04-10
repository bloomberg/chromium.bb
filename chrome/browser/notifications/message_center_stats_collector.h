// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_STATS_COLLECTOR_H_
#define CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_STATS_COLLECTOR_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/message_center_types.h"

namespace message_center {
class MessageCenter;
}

// MessageCenterStatsCollector sends both raw and per-notification statistics
// to the UMA servers, if the user has opted in.  It observes the message center
// to gather its data.
class MessageCenterStatsCollector
    : public message_center::MessageCenterObserver {
 public:
  enum NotificationActionType {
    NOTIFICATION_ACTION_UNKNOWN,
    NOTIFICATION_ACTION_ADD,
    NOTIFICATION_ACTION_UPDATE,
    NOTIFICATION_ACTION_CLICK,
    NOTIFICATION_ACTION_BUTTON_CLICK,
    NOTIFICATION_ACTION_DISPLAY,
    NOTIFICATION_ACTION_CLOSE_BY_USER,
    NOTIFICATION_ACTION_CLOSE_BY_SYSTEM,
    // NOTE: Add new action types only immediately above this line. Also,
    // make sure the enum list in tools/histogram/histograms.xml is
    // updated with any change in here.
    NOTIFICATION_ACTION_COUNT
  };

  explicit MessageCenterStatsCollector(
      message_center::MessageCenter* message_center);
  ~MessageCenterStatsCollector() override;

 private:
  // Represents the aggregate stats for each notification.
  class NotificationStats {
   public:
    // Default constructor for map.
    NotificationStats();

    explicit NotificationStats(const std::string& id);
    virtual ~NotificationStats();

    // Called when we get an action from the message center.
    void CollectAction(NotificationActionType type);

    // Sends aggregate data to UMA.
    void RecordAggregateStats();

   private:
    std::string id_;
    bool actions_[NOTIFICATION_ACTION_COUNT];
  };

  // MessageCenterObserver
  void OnNotificationAdded(const std::string& notification_id) override;
  void OnNotificationRemoved(const std::string& notification_id,
                             bool by_user) override;
  void OnNotificationUpdated(const std::string& notification_id) override;
  void OnNotificationClicked(const std::string& notification_id) override;
  void OnNotificationButtonClicked(const std::string& notification_id,
                                   int button_index) override;
  void OnNotificationSettingsClicked(bool handled) override;
  void OnNotificationDisplayed(
      const std::string& notification_id,
      const message_center::DisplaySource source) override;
  void OnCenterVisibilityChanged(
      message_center::Visibility visibility) override;
  void OnQuietModeChanged(bool in_quiet_mode) override;

  // Weak, global.
  message_center::MessageCenter* message_center_;

  typedef std::map<std::string,NotificationStats> StatsCollection;
  StatsCollection stats_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterStatsCollector);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_STATS_COLLECTOR_H_
