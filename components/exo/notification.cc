// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/notification.h"

#include <memory>

#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace exo {
namespace {

// Ref-counted delegate for handling events on notification with callbacks.
class NotificationDelegate : public message_center::NotificationDelegate {
 public:
  NotificationDelegate(
      const base::RepeatingCallback<void(bool)>& close_callback)
      : close_callback_(close_callback) {}

  // message_center::NotificationDelegate:
  void Close(bool by_user) override {
    if (!close_callback_)
      return;
    close_callback_.Run(by_user);
  }

 private:
  // The destructor is private since this class is ref-counted.
  ~NotificationDelegate() override = default;

  const base::RepeatingCallback<void(bool)> close_callback_;

  DISALLOW_COPY_AND_ASSIGN(NotificationDelegate);
};

}  // namespace

Notification::Notification(
    const std::string& title,
    const std::string& message,
    const std::string& display_source,
    const std::string& notification_id,
    const std::string& notifier_id,
    const base::RepeatingCallback<void(bool)>& close_callback)
    : notification_id_(notification_id) {
  auto notifier = message_center::NotifierId(
      message_center::NotifierId::APPLICATION, notifier_id);
  notifier.profile_id = ash::Shell::Get()
                            ->session_controller()
                            ->GetPrimaryUserSession()
                            ->user_info->account_id.GetUserEmail();

  auto notification = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      base::UTF8ToUTF16(title), base::UTF8ToUTF16(message), gfx::Image(),
      base::UTF8ToUTF16(display_source), GURL(), notifier,
      message_center::RichNotificationData(),
      base::MakeRefCounted<NotificationDelegate>(close_callback));

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

void Notification::Close() {
  message_center::MessageCenter::Get()->RemoveNotification(notification_id_,
                                                           false /* by_user */);
}

}  // namespace exo
