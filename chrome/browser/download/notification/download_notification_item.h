// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_NOTIFICATION_ITEM_H_
#define CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_NOTIFICATION_ITEM_H_

#include "chrome/browser/download/notification/download_notification_item.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/download/download_commands.h"
#include "content/public/browser/download_item.h"
#include "grit/theme_resources.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"

using message_center::Notification;

namespace test {
class DownloadNotificationItemTest;
}

class DownloadNotificationItem : public content::DownloadItem::Observer {
 public:
  class Delegate {
   public:
    virtual void OnDownloadStarted(DownloadNotificationItem* item) = 0;
    virtual void OnDownloadStopped(DownloadNotificationItem* item) = 0;
    virtual void OnDownloadRemoved(DownloadNotificationItem* item) = 0;
  };

  DownloadNotificationItem(content::DownloadItem* item, Delegate* delegate);

  ~DownloadNotificationItem() override;

 private:
  class NotificationWatcher : public message_center::NotificationDelegate,
                              public message_center::MessageCenterObserver {
   public:
    explicit NotificationWatcher(DownloadNotificationItem* item);

   private:
    ~NotificationWatcher() override;

    // message_center::NotificationDelegate overrides:
    void Close(bool by_user) override;
    void Click() override;
    bool HasClickedListener() override;
    void ButtonClick(int button_index) override;

    // message_center::MessageCenterObserver overrides:
    void OnNotificationRemoved(const std::string& id, bool by_user) override;

    DownloadNotificationItem* item_;
  };

  void OnNotificationClick();
  void OnNotificationButtonClick(int button_index);
  void OnNotificationClose(bool by_user);
  void OnNotificationRemoved(bool by_user);

  // DownloadItem::Observer overrides:
  void OnDownloadUpdated(content::DownloadItem* item) override;
  void OnDownloadOpened(content::DownloadItem* item) override;
  void OnDownloadRemoved(content::DownloadItem* item) override;
  void OnDownloadDestroyed(content::DownloadItem* item) override;

  void UpdateNotificationData();
  void SetNotificationImage(int resource_id);

  // Returns a short one-line status string for the download.
  base::string16 GetTitle() const;

  // Returns a short one-line status string for a download command.
  base::string16 GetCommandLabel(DownloadCommands::Command command) const;

  // Get the warning test to notify a dangerous download. Should only be called
  // if IsDangerous() is true.
  base::string16 GetWarningText() const;

  scoped_ptr<std::vector<DownloadCommands::Command>> GetPossibleActions() const;

  bool openable_;
  bool downloading_;
  bool reshow_after_remove_;
  int image_resource_id_;
  scoped_refptr<NotificationWatcher> watcher_;

  message_center::MessageCenter* message_center_;
  scoped_ptr<Notification> notification_;

  content::DownloadItem* item_;
  scoped_ptr<std::vector<DownloadCommands::Command>> button_actions_;

  Delegate* const delegate_;

  friend class test::DownloadNotificationItemTest;

  DISALLOW_COPY_AND_ASSIGN(DownloadNotificationItem);
};

#endif  // CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_NOTIFICATION_ITEM_H_
