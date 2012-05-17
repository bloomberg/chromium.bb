// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_NOTIFICATIONS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_NOTIFICATIONS_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "chrome/browser/chromeos/notifications/system_notification.h"

class Profile;

class FileBrowserNotifications
    : public base::SupportsWeakPtr<FileBrowserNotifications> {
 public:
  enum NotificationType {
    DEVICE,
    DEVICE_FAIL,
    FORMAT_SUCCESS,
    FORMAT_FAIL,
    FORMAT_START,
    FORMAT_START_FAIL,
    GDATA_SYNC,
    GDATA_SYNC_SUCCESS,
    GDATA_SYNC_FAIL,
  };

  typedef std::map<std::string, linked_ptr<chromeos::SystemNotification> >
      NotificationMap;

  explicit FileBrowserNotifications(Profile* profile);
  virtual ~FileBrowserNotifications();

  void RegisterDevice(const std::string& path);
  void UnregisterDevice(const std::string& path);

  void ManageNotificationsOnMountCompleted(const std::string& system_path,
                                           const std::string& label,
                                           bool is_parent,
                                           bool success,
                                           bool is_unsupported);

  void ManageNotificationOnGDataSyncProgress(int count);
  void ManageNotificationOnGDataSyncFinish(bool success);

  void ShowNotification(NotificationType type, const std::string& path);
  void ShowNotificationDelayed(NotificationType type,
                               const std::string& path,
                               base::TimeDelta delay);
  virtual void ShowNotificationWithMessage(NotificationType type,
                                           const std::string& path,
                                           const string16& message);

  virtual void HideNotification(NotificationType type, const std::string& path);
  void HideNotificationDelayed(NotificationType type,
                               const std::string& path,
                               base::TimeDelta delay);

  const NotificationMap& notifications() const { return notifications_; }

 protected:
  virtual void PostDelayedShowNotificationTask(
      const std::string& notification_id,
      NotificationType type,
      const string16&  message,
      base::TimeDelta delay);
  static void ShowNotificationDelayedTask(const std::string& notification_id,
      NotificationType type,
      const string16& message,
      base::WeakPtr<FileBrowserNotifications> self);

  virtual void PostDelayedHideNotificationTask(
      NotificationType type, const std::string path, base::TimeDelta delay);
  static void HideNotificationDelayedTask(NotificationType type,
      const std::string& path,
      base::WeakPtr<FileBrowserNotifications> self);

 private:
  struct MountRequestsInfo {
    bool mount_success_exists;
    bool fail_message_finalized;
    bool fail_notification_shown;
    bool non_parent_device_failed;
    bool device_notification_hidden;
    int fail_notifications_count;

    MountRequestsInfo() : mount_success_exists(false),
                          fail_message_finalized(false),
                          fail_notification_shown(false),
                          non_parent_device_failed(false),
                          device_notification_hidden(false),
                          fail_notifications_count(0) {
    }
  };

  typedef std::map<std::string, MountRequestsInfo> MountRequestsMap;

  chromeos::SystemNotification* CreateNotification(const std::string& id,
                                                   int title_id,
                                                   int icon_id);

  void CreateNotificationId(NotificationType type,
                            const std::string& path,
                            std::string* id);
  int GetMessageId(NotificationType type);
  int GetIconId(NotificationType type);
  int GetTitleId(NotificationType type);

  void OnLinkClicked(const base::ListValue* arg);
  bool HasMoreInfoLink(NotificationType type);
  const string16& GetLinkText();
  chromeos::BalloonViewHost::MessageCallback GetLinkCallback();

  string16 link_text_;
  NotificationMap notifications_;
  MountRequestsMap mount_requests_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(FileBrowserNotifications);
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_NOTIFICATIONS_H_
