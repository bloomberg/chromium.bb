// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_NOTIFICATION_PRESENTER_H_
#define CHROMEOS_COMPONENTS_TETHER_NOTIFICATION_PRESENTER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"

namespace message_center {
class MessageCenter;
class Notification;
}  // namespace message_center

namespace chromeos {

namespace tether {

class NotificationPresenter {
 public:
  NotificationPresenter();
  virtual ~NotificationPresenter();

  // Notifies the user that the connection attempt has failed.
  virtual void NotifyConnectionToHostFailed(
      const std::string& host_device_name);

  // Removes the notification created by NotifyConnectionToHostFailed(), or does
  // nothing if that notification is not currently displayed.
  virtual void RemoveConnectionToHostFailedNotification();

 protected:
  NotificationPresenter(message_center::MessageCenter* message_center);

 private:
  friend class NotificationPresenterTest;

  static const char kTetherNotifierId[];
  static const char kActiveHostNotificationId[];

  static std::unique_ptr<message_center::Notification> CreateNotification(
      const std::string& id,
      const base::string16& title,
      const base::string16& message);

  message_center::MessageCenter* message_center_;

  base::WeakPtrFactory<NotificationPresenter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NotificationPresenter);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_NOTIFICATION_PRESENTER_H_
