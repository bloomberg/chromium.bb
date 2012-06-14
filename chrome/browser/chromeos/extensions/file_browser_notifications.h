// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_NOTIFICATIONS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_NOTIFICATIONS_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "base/time.h"

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

  // Retreives message body based on |type|.
  void ShowNotification(NotificationType type, const std::string& path);
  // Primary method for showing a notification. Virtual for mock in unittest.
  virtual void ShowNotificationWithMessage(NotificationType type,
                                           const std::string& path,
                                           const string16& message);
  void ShowNotificationDelayed(NotificationType type,
                               const std::string& path,
                               base::TimeDelta delay);

  // Primary method for hiding a notification. Virtual for mock in unittest.
  virtual void HideNotification(NotificationType type, const std::string& path);
  void HideNotificationDelayed(NotificationType type,
                               const std::string& path,
                               base::TimeDelta delay);

  size_t GetNotificationCountForTest() const {
    return notification_map_.size();
  }
  bool HasNotificationForTest(const std::string& id) const {
    return notification_map_.find(id) != notification_map_.end();
  }

 private:
  class NotificationMessage;
  struct MountRequestsInfo;

  typedef std::map<std::string, MountRequestsInfo> MountRequestsMap;
  typedef std::map<std::string, NotificationMessage*> NotificationMap;

  std::string CreateNotificationId(NotificationType type,
                                   const std::string& path);
  NotificationMessage* GetNotification(NotificationType type,
                                       const std::string& path,
                                       const string16& message);
  void ShowNotificationById(NotificationType type,
                            const std::string& notification_id,
                            const string16& message);
  void HideNotificationById(const std::string& id);

  NotificationMap notification_map_;
  std::set<std::string> hidden_notifications_;
  MountRequestsMap mount_requests_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(FileBrowserNotifications);
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_NOTIFICATIONS_H_
