// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_error_observer_chromeos.h"

#include <utility>

#include "ash/system/system_notifier.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"
#include "ui/message_center/notification_list.h"

using message_center::Notification;

namespace ash {
namespace {

const char kDisplayErrorNotificationId[] = "chrome://settings/display/error";

}  // namespace

DisplayErrorObserver::DisplayErrorObserver() {
}

DisplayErrorObserver::~DisplayErrorObserver() {
}

void DisplayErrorObserver::OnDisplayModeChangeFailed(
    const ui::DisplayConfigurator::DisplayStateList& displays,
    ui::MultipleDisplayState new_state) {
  // Always remove the notification to make sure the notification appears
  // as a popup in any situation.
  message_center::MessageCenter::Get()->RemoveNotification(
      kDisplayErrorNotificationId, false /* by_user */);

  int message_id = (new_state == ui::MULTIPLE_DISPLAY_STATE_DUAL_MIRROR) ?
      IDS_ASH_DISPLAY_FAILURE_ON_MIRRORING :
      IDS_ASH_DISPLAY_FAILURE_ON_NON_MIRRORING;

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  scoped_ptr<Notification> notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kDisplayErrorNotificationId,
      base::string16(),  // title
      l10n_util::GetStringUTF16(message_id),
      bundle.GetImageNamed(IDR_AURA_NOTIFICATION_DISPLAY),
      base::string16(),  // display_source
      GURL(),
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 system_notifier::kNotifierDisplayError),
      message_center::RichNotificationData(), NULL));
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

base::string16
DisplayErrorObserver::GetDisplayErrorNotificationMessageForTest() {
  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  for (message_center::NotificationList::Notifications::const_iterator iter =
           notifications.begin(); iter != notifications.end(); ++iter) {
    if ((*iter)->id() == kDisplayErrorNotificationId)
      return (*iter)->message();
  }

  return base::string16();
}

}  // namespace ash
