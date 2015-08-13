// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_GROUP_NOTIFICATION_H_
#define CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_GROUP_NOTIFICATION_H_

#include <set>

#include "chrome/browser/download/notification/download_notification.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/download_item.h"

class DownloadNotificationManagerForProfile;

class DownloadGroupNotification : public DownloadNotification {
 public:
  DownloadGroupNotification(
      Profile* profile, DownloadNotificationManagerForProfile* manager);
  ~DownloadGroupNotification() override;

  bool IsPopup() const;
  void OnDownloadAdded(content::DownloadItem* download);

  // Methods called from NotificationWatcher.
  void OnDownloadUpdated(content::DownloadItem* download) override;
  void OnDownloadRemoved(content::DownloadItem* download) override;
  void OnNotificationClose() override;
  void OnNotificationClick() override;
  void OnNotificationButtonClick(int button_index) override;
  std::string GetNotificationId() const override;

  bool visible() const { return visible_; }

 private:
  struct FilenameCache {
    base::FilePath original_filename;
    base::string16 truncated_filename;
  };

  void Update();
  void UpdateNotificationData();

  void Hide();
  void Show();

  void OpenDownloads();

  Profile* profile_ = nullptr;
  // Flag to show the notification on the next update.
  bool show_next_ = false;
  // Hides the notification on the next udpate.
  bool hide_next_ = false;
  // Current vilibility status of the notification.
  bool visible_ = false;

  scoped_ptr<Notification> notification_;
  std::set<content::DownloadItem*> items_;
  std::map<content::DownloadItem*, FilenameCache> truncated_filename_cache_;

  DISALLOW_COPY_AND_ASSIGN(DownloadGroupNotification);
};

#endif  // CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_GROUP_NOTIFICATION_H_
