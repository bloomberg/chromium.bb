// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/notification_presenter.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/proximity_auth/logging/logging.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/notifier_settings.h"

namespace chromeos {

namespace tether {

// static
constexpr const char NotificationPresenter::kTetherNotifierId[] =
    "cros_tether_notification_ids.notifier_id";

// static
constexpr const char NotificationPresenter::kActiveHostNotificationId[] =
    "cros_tether_notification_ids.active_host";

// static
std::unique_ptr<message_center::Notification>
NotificationPresenter::CreateNotification(const std::string& id,
                                          const base::string16& title,
                                          const base::string16& message) {
  return base::MakeUnique<message_center::Notification>(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE, id, title,
      message,
      // TODO(khorimoto): Add tether icon.
      gfx::Image() /* icon */, base::string16() /* display_source */,
      GURL() /* origin_url */,
      message_center::NotifierId(
          message_center::NotifierId::NotifierType::SYSTEM_COMPONENT,
          kTetherNotifierId),
      message_center::RichNotificationData(), nullptr);
}

NotificationPresenter::NotificationPresenter()
    : NotificationPresenter(message_center::MessageCenter::Get()) {}

NotificationPresenter::~NotificationPresenter() {}

NotificationPresenter::NotificationPresenter(
    message_center::MessageCenter* message_center)
    : message_center_(message_center), weak_ptr_factory_(this) {}

void NotificationPresenter::NotifyConnectionToHostFailed(
    const std::string& host_device_name) {
  PA_LOG(INFO) << "Displaying \"connection attempt failed\" notification for "
               << "device with name \"" << host_device_name << "\".";

  // TODO(khorimoto): Use real strings and localize them.
  message_center_->AddNotification(CreateNotification(
      std::string(kActiveHostNotificationId),
      base::ASCIIToUTF16("Instant Tethering connection failed"),
      base::ASCIIToUTF16("Tap to configure")));
}

void NotificationPresenter::RemoveConnectionToHostFailedNotification() {
  PA_LOG(INFO) << "Removing \"connection attempt failed\" dialog.";

  message_center_->RemoveNotification(std::string(kActiveHostNotificationId),
                                      false /* by_user */);
}

}  // namespace tether

}  // namespace chromeos
