// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_NOTIFICATION_MANAGER_H_

#include <memory>
#include <set>

#include "chrome/browser/download/download_ui_controller.h"
#include "chrome/browser/download/notification/download_item_notification.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/download_item.h"

class DownloadNotificationManagerForProfile;

class DownloadNotificationManager : public DownloadUIController::Delegate {
 public:
  explicit DownloadNotificationManager(Profile* profile);
  ~DownloadNotificationManager() override;

  void OnAllDownloadsRemoving(Profile* profile);
  // DownloadUIController::Delegate:
  void OnNewDownloadReady(content::DownloadItem* item) override;

  DownloadNotificationManagerForProfile* GetForProfile(Profile* profile) const;

 private:
  friend class test::DownloadItemNotificationTest;

  Profile* main_profile_ = nullptr;
  std::map<Profile*, std::unique_ptr<DownloadNotificationManagerForProfile>>
      manager_for_profile_;
};

class DownloadNotificationManagerForProfile
    : public content::DownloadItem::Observer {
 public:
  DownloadNotificationManagerForProfile(
      Profile* profile, DownloadNotificationManager* parent_manager);
  ~DownloadNotificationManagerForProfile() override;

  message_center::MessageCenter* message_center() const {
    return message_center_;
  }

  // DownloadItem::Observer overrides:
  void OnDownloadUpdated(content::DownloadItem* download) override;
  void OnDownloadOpened(content::DownloadItem* download) override;
  void OnDownloadRemoved(content::DownloadItem* download) override;
  void OnDownloadDestroyed(content::DownloadItem* download) override;

  void OnNewDownloadReady(content::DownloadItem* item);

 private:
  friend class test::DownloadItemNotificationTest;

  void OverrideMessageCenterForTest(
      message_center::MessageCenter* message_center);

  Profile* profile_ = nullptr;
  DownloadNotificationManager* parent_manager_;  // weak
  std::set<content::DownloadItem*> downloading_items_;
  std::map<content::DownloadItem*, std::unique_ptr<DownloadItemNotification>>
      items_;

  // Pointer to the message center instance.
  message_center::MessageCenter* message_center_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_NOTIFICATION_MANAGER_H_
