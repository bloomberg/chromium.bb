// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_NOTIFICATION_MANAGER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/file_system_provider/notification_manager_interface.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/extensions/app_icon_loader.h"

class Profile;

namespace gfx {
class Image;
class ImageSkia;
}  // message gfx

namespace message_center {
class Notification;
}  // namespace message_center

namespace chromeos {
namespace file_system_provider {

// Provided file systems's manager for showing notifications. Shows always
// up to one notification. If more than one request is unresponsive, then
// all of them will be aborted when clicking on the notification button.
class NotificationManager : public NotificationManagerInterface,
                            public extensions::AppIconLoader::Delegate {
 public:
  NotificationManager(Profile* profile,
                      const ProvidedFileSystemInfo& file_system_info);
  virtual ~NotificationManager();

  // NotificationManagerInterface overrides:
  virtual void ShowUnresponsiveNotification(
      int id,
      const NotificationCallback& callback) OVERRIDE;
  virtual void HideUnresponsiveNotification(int id) OVERRIDE;

  // Invoked when a button on the notification is clicked.
  void OnButtonClick(int button_index);

  // Invoked when the notification failed to show up.
  void OnError();

  // Invoked when the notification got closed either by user or by system.
  void OnClose();

  // extensions::AppIconLoader::Delegate overrides:
  virtual void SetAppImage(const std::string& id,
                           const gfx::ImageSkia& image) OVERRIDE;

 private:
  typedef std::map<int, NotificationCallback> CallbackMap;

  // Creates a notification object for the actual state of the manager.
  scoped_ptr<message_center::Notification> CreateNotification();

  // Handles a notification result by calling all registered callbacks and
  // clearing the list.
  void OnNotificationResult(NotificationResult result);

  Profile* profile_;  // Not owned.
  ProvidedFileSystemInfo file_system_info_;
  CallbackMap callbacks_;
  scoped_ptr<extensions::AppIconLoader> icon_loader_;
  scoped_ptr<gfx::Image> extension_icon_;

  DISALLOW_COPY_AND_ASSIGN(NotificationManager);
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_NOTIFICATION_MANAGER_H_
