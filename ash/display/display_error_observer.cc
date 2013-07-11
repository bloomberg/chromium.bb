// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_error_observer.h"

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
namespace internal {
namespace {

const char kDisplayErrorNotificationId[] = "chrome://settings/display/error";

class DisplayErrorNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  DisplayErrorNotificationDelegate() {}

  // message_center::NotificationDelegate overrides:
  virtual void Display() OVERRIDE {}
  virtual void Error() OVERRIDE {}
  virtual void Close(bool by_user) OVERRIDE {}
  virtual bool HasClickedListener() OVERRIDE { return false; }
  virtual void Click() OVERRIDE { }

 protected:
  virtual ~DisplayErrorNotificationDelegate() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayErrorNotificationDelegate);
};

}  // namespace

DisplayErrorObserver::DisplayErrorObserver() {
}

DisplayErrorObserver::~DisplayErrorObserver() {
}

void DisplayErrorObserver::OnDisplayModeChangeFailed(
    chromeos::OutputState new_state) {
  // Always remove the notification to make sure the notification appears
  // as a popup in any situation.
  message_center::MessageCenter::Get()->RemoveNotification(
      kDisplayErrorNotificationId, false /* by_user */);

  int message_id = (new_state == chromeos::STATE_DUAL_MIRROR) ?
      IDS_ASH_DISPLAY_FAILURE_ON_MIRRORING :
      IDS_ASH_DISPLAY_FAILURE_ON_NON_MIRRORING;

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  scoped_ptr<Notification> notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      kDisplayErrorNotificationId,
      l10n_util::GetStringUTF16(message_id),
      base::string16(),  // message
      bundle.GetImageNamed(IDR_AURA_UBER_TRAY_DISPLAY),
      base::string16(),  // display_source
      std::string(),  // extension_id
      message_center::RichNotificationData(),
      new DisplayErrorNotificationDelegate()));
  message_center::MessageCenter::Get()->AddNotification(notification.Pass());
}

string16 DisplayErrorObserver::GetTitleOfDisplayErrorNotificationForTest() {
  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetNotifications();
  for (message_center::NotificationList::Notifications::const_iterator iter =
           notifications.begin(); iter != notifications.end(); ++iter) {
    if ((*iter)->id() == kDisplayErrorNotificationId)
      return (*iter)->title();
  }

  return base::string16();
}

}  // namespace internal
}  // namespace ash
