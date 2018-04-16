// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/supervised/supervised_notification_controller.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

using base::UTF8ToUTF16;
using message_center::MessageCenter;
using message_center::Notification;

namespace ash {

namespace {
const char kNotifierSupervisedUser[] = "ash.locally-managed-user";
}  // namespace

const char SupervisedNotificationController::kNotificationId[] =
    "chrome://user/locally-managed";

SupervisedNotificationController::SupervisedNotificationController() = default;

SupervisedNotificationController::~SupervisedNotificationController() = default;

// static
base::string16 SupervisedNotificationController::GetSupervisedUserMessage() {
  SessionController* session_controller = Shell::Get()->session_controller();
  DCHECK(session_controller->IsUserSupervised());
  DCHECK(session_controller->IsActiveUserSessionStarted());

  // Get the active user session.
  const mojom::UserSession* const user_session =
      session_controller->GetUserSession(0);
  DCHECK(user_session);

  base::string16 first_custodian = UTF8ToUTF16(user_session->custodian_email);
  base::string16 second_custodian =
      UTF8ToUTF16(user_session->second_custodian_email);

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

void SupervisedNotificationController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  OnUserSessionUpdated(account_id);
}

void SupervisedNotificationController::OnUserSessionAdded(
    const AccountId& account_id) {
  OnUserSessionUpdated(account_id);
}

void SupervisedNotificationController::OnUserSessionUpdated(
    const AccountId& account_id) {
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

// static
void SupervisedNotificationController::CreateOrUpdateNotification() {
  // No notification for the child user.
  if (Shell::Get()->session_controller()->IsUserChild())
    return;

  // Regular supervised user.
  std::unique_ptr<Notification> notification =
      Notification::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId,
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SUPERVISED_LABEL),
          GetSupervisedUserMessage(), gfx::Image(),
          base::string16() /* display_source */, GURL(),
          message_center::NotifierId(
              message_center::NotifierId::SYSTEM_COMPONENT,
              kNotifierSupervisedUser),
          message_center::RichNotificationData(), nullptr,
          kNotificationSupervisedIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);
  notification->SetSystemPriority();
  // AddNotification does an update if the notification already exists.
  MessageCenter::Get()->AddNotification(std::move(notification));
}

}  // namespace ash
