// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/supervised/tray_supervised_user.h"

#include <utility>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/system_notifier.h"
#include "ash/system/tray/label_tray_view.h"
#include "ash/system/tray/tray_constants.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"

using base::UTF8ToUTF16;
using message_center::MessageCenter;
using message_center::Notification;

namespace ash {
namespace {

const gfx::VectorIcon& GetSupervisedUserIcon() {
  SessionController* session_controller = Shell::Get()->session_controller();
  DCHECK(session_controller->IsUserSupervised());

  if (session_controller->IsUserChild())
    return kSystemMenuChildUserIcon;

  return kSystemMenuSupervisedUserIcon;
}

}  // namespace

const char TraySupervisedUser::kNotificationId[] =
    "chrome://user/locally-managed";

TraySupervisedUser::TraySupervisedUser(SystemTray* system_tray)
    : SystemTrayItem(system_tray, UMA_SUPERVISED_USER),
      scoped_session_observer_(this) {}

TraySupervisedUser::~TraySupervisedUser() = default;

views::View* TraySupervisedUser::CreateDefaultView(LoginStatus status) {
  if (!Shell::Get()->session_controller()->IsUserSupervised())
    return nullptr;

  LabelTrayView* tray_view =
      new LabelTrayView(nullptr, GetSupervisedUserIcon());
  // The message almost never changes during a session, so we compute it when
  // the menu is shown. We don't update it while the menu is open.
  tray_view->SetMessage(GetSupervisedUserMessage());
  return tray_view;
}

void TraySupervisedUser::OnUserSessionUpdated(const AccountId& account_id) {
  // NOTE: This doesn't use OnUserSessionAdded() because the custodian info
  // isn't available until after the session starts.
  SessionController* session_controller = Shell::Get()->session_controller();
  if (!session_controller->IsUserSupervised())
    return;

  // Get the active user session.
  DCHECK(session_controller->IsActiveUserSessionStarted());
  const mojom::UserSession* const user_session =
      session_controller->GetUserSession(0);
  DCHECK(user_session);

  // Only respond to updates for the active user.
  if (user_session->user_info->account_id != account_id)
    return;

  // Show notifications when custodian data first becomes available on login
  // and if the custodian data changes.
  if (custodian_email_ == user_session->custodian_email &&
      second_custodian_email_ == user_session->second_custodian_email) {
    return;
  }
  custodian_email_ = user_session->custodian_email;
  second_custodian_email_ = user_session->second_custodian_email;

  CreateOrUpdateNotification();
}

void TraySupervisedUser::CreateOrUpdateNotification() {
  std::unique_ptr<Notification> notification =
      Notification::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId,
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SUPERVISED_LABEL),
          GetSupervisedUserMessage(), gfx::Image(),
          base::string16() /* display_source */, GURL(),
          message_center::NotifierId(
              message_center::NotifierId::SYSTEM_COMPONENT,
              system_notifier::kNotifierSupervisedUser),
          message_center::RichNotificationData(), nullptr,
          kNotificationSupervisedIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);
  notification->SetSystemPriority();
  // AddNotification does an update if the notification already exists.
  MessageCenter::Get()->AddNotification(std::move(notification));
}

base::string16 TraySupervisedUser::GetSupervisedUserMessage() const {
  base::string16 first_custodian = UTF8ToUTF16(custodian_email_);
  base::string16 second_custodian = UTF8ToUTF16(second_custodian_email_);

  // Regular supervised user. The "manager" is the first custodian.
  if (!Shell::Get()->session_controller()->IsUserChild()) {
    return l10n_util::GetStringFUTF16(IDS_ASH_USER_IS_SUPERVISED_BY_NOTICE,
                                      first_custodian);
  }

  // Child supervised user.
  if (second_custodian.empty()) {
    return l10n_util::GetStringFUTF16(
        IDS_ASH_CHILD_USER_IS_MANAGED_BY_ONE_PARENT_NOTICE, first_custodian);
  }
  return l10n_util::GetStringFUTF16(
      IDS_ASH_CHILD_USER_IS_MANAGED_BY_TWO_PARENTS_NOTICE, first_custodian,
      second_custodian);
}

}  // namespace ash
