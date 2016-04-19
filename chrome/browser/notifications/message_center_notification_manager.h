// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_NOTIFICATION_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/notifications/google_now_notification_stats_collector.h"
#include "chrome/browser/notifications/message_center_stats_collector.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_system_observer.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/message_center_tray_delegate.h"
#include "ui/message_center/message_center_types.h"

class MessageCenterSettingsController;
class Notification;
class Profile;
class ProfileNotification;

namespace message_center {
class NotificationBlocker;
FORWARD_DECLARE_TEST(WebNotificationTrayTest, ManuallyCloseMessageCenter);
}

// This class extends NotificationUIManagerImpl and delegates actual display
// of notifications to MessageCenter, doing necessary conversions.
class MessageCenterNotificationManager
    : public NotificationUIManager,
      public message_center::MessageCenterObserver {
 public:
  MessageCenterNotificationManager(
      message_center::MessageCenter* message_center,
      std::unique_ptr<message_center::NotifierSettingsProvider>
          settings_provider);
  ~MessageCenterNotificationManager() override;

  // NotificationUIManager
  void Add(const Notification& notification, Profile* profile) override;
  bool Update(const Notification& notification, Profile* profile) override;
  const Notification* FindById(const std::string& delegate_id,
                               ProfileID profile_id) const override;
  bool CancelById(const std::string& delegate_id,
                  ProfileID profile_id) override;
  std::set<std::string> GetAllIdsByProfileAndSourceOrigin(
      ProfileID profile_id,
      const GURL& source) override;
  std::set<std::string> GetAllIdsByProfile(ProfileID profile_id) override;
  bool CancelAllBySourceOrigin(const GURL& source_origin) override;
  bool CancelAllByProfile(ProfileID profile_id) override;
  void CancelAll() override;

  // MessageCenterObserver
  void OnNotificationRemoved(const std::string& notification_id,
                             bool by_user) override;
  void OnCenterVisibilityChanged(message_center::Visibility) override;
  void OnNotificationUpdated(const std::string& notification_id) override;

  void EnsureMessageCenterClosed();

  // Takes ownership of |delegate|.
  void SetMessageCenterTrayDelegateForTest(
      message_center::MessageCenterTrayDelegate* delegate);

  // Returns the notification id which this manager will use to add to message
  // center, for this combination of delegate id and profile.
  std::string GetMessageCenterNotificationIdForTest(
      const std::string& delegate_id, Profile* profile);

 private:
  // Adds |profile_notification| to an alternative provider extension or app.
  void AddNotificationToAlternateProvider(
      const Notification& notification,
      Profile* profile,
      const std::string& extension_id) const;

  FRIEND_TEST_ALL_PREFIXES(message_center::WebNotificationTrayTest,
                           ManuallyCloseMessageCenter);

  std::unique_ptr<message_center::MessageCenterTrayDelegate> tray_;
  message_center::MessageCenter* message_center_;  // Weak, global.

  // Use a map by notification_id since this mapping is the most often used.
  typedef std::map<std::string, ProfileNotification*> NotificationMap;
  NotificationMap profile_notifications_;

  // Helpers that add/remove the notification from local map.
  // The local map takes ownership of profile_notification object.
  void AddProfileNotification(ProfileNotification* profile_notification);
  void RemoveProfileNotification(ProfileNotification* profile_notification);

  // Returns the ProfileNotification for the |id|, or NULL if no such
  // notification is found.
  ProfileNotification* FindProfileNotification(const std::string& id) const;

  // Get the extension ID of the extension that the user chose to take over
  // Chorme Notification Center.
  std::string GetExtensionTakingOverNotifications(Profile* profile);

  std::unique_ptr<message_center::NotifierSettingsProvider> settings_provider_;

  // To own the blockers.
  std::vector<std::unique_ptr<message_center::NotificationBlocker>> blockers_;

  NotificationSystemObserver system_observer_;

  // Keeps track of all notification statistics for UMA purposes.
  MessageCenterStatsCollector stats_collector_;

  // Keeps track of notifications specific to Google Now for UMA purposes.
  GoogleNowNotificationStatsCollector google_now_stats_collector_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterNotificationManager);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_NOTIFICATION_MANAGER_H_
