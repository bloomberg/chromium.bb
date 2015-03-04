// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_NOTIFICATION_MANAGER_H_

#include <set>

#include "chrome/browser/download/download_ui_controller.h"
#include "chrome/browser/download/notification/download_notification_item.h"
#include "chrome/browser/profiles/profile.h"

class Profile;

class DownloadNotificationManager : public DownloadUIController::Delegate,
                                    public DownloadNotificationItem::Delegate {
 public:
  explicit DownloadNotificationManager(Profile* profile);
  ~DownloadNotificationManager() override;

  // DownloadUIController::Delegate:
  void OnNewDownloadReady(content::DownloadItem* item) override;

  // DownloadNotificationItem::Delegate:
  void OnDownloadStarted(DownloadNotificationItem* item) override;
  void OnDownloadStopped(DownloadNotificationItem* item) override;
  void OnDownloadRemoved(DownloadNotificationItem* item) override;

 private:
  std::set<DownloadNotificationItem*> downloading_items_;
  std::set<DownloadNotificationItem*> items_;

  STLElementDeleter<std::set<DownloadNotificationItem*>> items_deleter_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_NOTIFICATION_MANAGER_H_
