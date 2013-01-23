// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_NOTIFICATION_MANAGER_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/notifications/notification_ui_manager_impl.h"
#include "ui/message_center/message_center.h"

class Notification;
class Profile;

// This class extends NotificationUIManagerImpl and delegates actual display
// of notifications to MessageCenter, doing necessary conversions.
class MessageCenterNotificationManager
    : public NotificationUIManagerImpl,
      public message_center::MessageCenter::Delegate {
 public:
  explicit MessageCenterNotificationManager(
      message_center::MessageCenter* message_center);
  virtual ~MessageCenterNotificationManager();

  // NotificationUIManager
  virtual bool CancelById(const std::string& notification_id) OVERRIDE;
  virtual bool CancelAllBySourceOrigin(const GURL& source_origin) OVERRIDE;
  virtual bool CancelAllByProfile(Profile* profile) OVERRIDE;
  virtual void CancelAll() OVERRIDE;

  // NotificationUIManagerImpl
  virtual bool ShowNotification(const Notification& notification,
                                Profile* profile) OVERRIDE;
  virtual bool UpdateNotification(const Notification& notification,
                                  Profile* profile) OVERRIDE;

  // MessageCenter::Delegate
  virtual void NotificationRemoved(const std::string& notification_id) OVERRIDE;
  virtual void DisableExtension(const std::string& notification_id) OVERRIDE;
  virtual void DisableNotificationsFromSource(
      const std::string& notification_id) OVERRIDE;
  virtual void ShowSettings(const std::string& notification_id) OVERRIDE;
  virtual void OnClicked(const std::string& notification_id) OVERRIDE;
  virtual void OnButtonClicked(const std::string& notification_id,
                               int button_index) OVERRIDE;

 private:
  message_center::MessageCenter* message_center_;  // Weak, global.

// This class keeps a set of original Notification objects and corresponding
// Profiles, so when MessageCenter calls back with a notification_id, this
// class has necessary mapping to other source info - for example, it calls
// NotificationDelegate supplied by client when someone clicks on a Notification
// in MessageCenter. Likewise, if a Profile or Extension is being removed, the
// map makes it possible to revoke the notifications from MessageCenter.
// To keep that set, we use the private ProfileNotification class that stores
// a superset of all information about a notification.

// TODO(dimich): Consider merging all 4 types (Notification, QueuedNotification,
// ProfileNotification and NotificationList::Notification) into a single class.
  class ProfileNotification {
   public:
    ProfileNotification(Profile* profile, const Notification& notification);

    Profile* profile() const { return profile_; }
    const Notification& notification() const { return notification_; }

    // Returns extension_id if the notification originates from an extension,
    // empty string otherwise.
    std::string GetExtensionId();

   private:
    // Weak, guaranteed not to be used after profile removal by parent class.
    Profile* profile_;
    Notification notification_;
  };

  // Use a map by notification_id since this mapping is the most often used.
  typedef std::map<std::string, ProfileNotification*> NotificationMap;
  NotificationMap profile_notifications_;

  // Helpers that add/remove the notification from local map and MessageCenter.
  // They take ownership of profile_notification object.
  void AddProfileNotification(ProfileNotification* profile_notification);
  void RemoveProfileNotification(ProfileNotification* profile_notification);

  DISALLOW_COPY_AND_ASSIGN(MessageCenterNotificationManager);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_NOTIFICATION_MANAGER_H_


