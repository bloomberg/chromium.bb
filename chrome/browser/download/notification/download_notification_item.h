// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_NOTIFICATION_ITEM_H_
#define CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_NOTIFICATION_ITEM_H_

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/download/notification/download_notification.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "content/public/browser/download_item.h"
#include "grit/theme_resources.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"

namespace test {
class DownloadNotificationItemTest;
}

class DownloadNotificationItem : public DownloadNotification {
 public:
  DownloadNotificationItem(content::DownloadItem* item,
                           DownloadNotificationManagerForProfile* manager);

  ~DownloadNotificationItem() override;

  // Methods called from NotificationWatcher.
  void OnDownloadUpdated(content::DownloadItem* item) override;
  void OnDownloadRemoved(content::DownloadItem* item) override;
  void OnNotificationClose() override;
  void OnNotificationClick() override;
  void OnNotificationButtonClick(int button_index) override;
  std::string GetNotificationId() const override;

 private:
  friend class test::DownloadNotificationItemTest;

  enum NotificationUpdateType {
    ADD,
    UPDATE,
    UPDATE_AND_POPUP
  };

  void CloseNotificationByUser();
  void CloseNotificationByNonUser();
  void Update();
  void UpdateNotificationData(NotificationUpdateType type);
  void SetNotificationImage(int resource_id);

  // Returns a short one-line status string for the download.
  base::string16 GetTitle() const;

  // Returns a short one-line status string for a download command.
  base::string16 GetCommandLabel(DownloadCommands::Command command) const;

  // Get the warning test to notify a dangerous download. Should only be called
  // if IsDangerous() is true.
  base::string16 GetWarningText() const;

  Browser* GetBrowser() const;
  Profile* profile() const;

  // Returns the list of possible extra (all except the default) actions.
  scoped_ptr<std::vector<DownloadCommands::Command>> GetExtraActions() const;

  // Flag to show the notification on next update. If true, the notification
  // goes visible. The initial value is true so it gets shown on initial update.
  bool show_next_ = true;
  // Current vilibility status of the notification.
  bool visible_ = false;

  int image_resource_id_ = 0;
  content::DownloadItem::DownloadState previous_download_state_ =
      content::DownloadItem::MAX_DOWNLOAD_STATE;  // As uninitialized state
  scoped_ptr<Notification> notification_;
  content::DownloadItem* item_;
  scoped_ptr<std::vector<DownloadCommands::Command>> button_actions_;

  DISALLOW_COPY_AND_ASSIGN(DownloadNotificationItem);
};

#endif  // CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_NOTIFICATION_ITEM_H_
