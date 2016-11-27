// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_NOTIFICATION_H_
#define CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_NOTIFICATION_H_

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "content/public/browser/download_item.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"

class DownloadNotification {
 public:
  DownloadNotification();
  virtual ~DownloadNotification();

  static const char kDownloadNotificationOrigin[];

  virtual void OnDownloadUpdated(content::DownloadItem* item) = 0;
  virtual void OnDownloadRemoved(content::DownloadItem* item) = 0;

  virtual bool HasNotificationClickedListener();
  virtual void OnNotificationClose() {}
  virtual void OnNotificationClick() {}
  virtual void OnNotificationButtonClick(int button_index) {}
  virtual std::string GetNotificationId() const = 0;

  // This method may break a layout of notification center. See the comment in
  // message_center::MessageCenter::ForceNotificationFlush() for detail.
  void InvokeUnsafeForceNotificationFlush(
      message_center::MessageCenter* message_center, const std::string& id);

 protected:
  NotificationDelegate* watcher() const;

 private:
  scoped_refptr<NotificationDelegate> watcher_;

  DISALLOW_COPY_AND_ASSIGN(DownloadNotification);
};

#endif  // CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_NOTIFICATION_H_
