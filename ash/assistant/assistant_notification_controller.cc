// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_notification_controller.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/new_window_controller.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/voice_interaction/voice_interaction_controller.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr char kNotifierId[] = "assistant";

// Helpers ---------------------------------------------------------------------

std::unique_ptr<message_center::Notification> CreateSystemNotification(
    const message_center::NotifierId& notifier_id,
    const chromeos::assistant::mojom::AssistantNotification* notification) {
  const base::string16 title = base::UTF8ToUTF16(notification->title);
  const base::string16 message = base::UTF8ToUTF16(notification->message);
  const base::string16 display_source =
      l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_NOTIFICATION_DISPLAY_SOURCE);

  message_center::RichNotificationData data;
  for (const auto& button : notification->buttons) {
    data.buttons.push_back(
        message_center::ButtonInfo(base::UTF8ToUTF16(button->label)));
  }

  std::unique_ptr<message_center::Notification> system_notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification->client_id,
          title, message, display_source, GURL(), notifier_id, data,
          /*delegate=*/nullptr, kNotificationAssistantIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);

  system_notification->set_priority(message_center::DEFAULT_PRIORITY);

  return system_notification;
}

message_center::NotifierId GetNotifierId() {
  return message_center::NotifierId(
      message_center::NotifierType::SYSTEM_COMPONENT, kNotifierId);
}

bool IsValidActionUrl(const GURL& action_url) {
  return action_url.is_valid() && (action_url.SchemeIsHTTPOrHTTPS() ||
                                   assistant::util::IsDeepLinkUrl(action_url));
}

}  // namespace

// AssistantNotificationController ---------------------------------------------

AssistantNotificationController::AssistantNotificationController(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller),
      binding_(this),
      notifier_id_(GetNotifierId()) {
  AddModelObserver(this);
  message_center::MessageCenter::Get()->AddObserver(this);
}

AssistantNotificationController::~AssistantNotificationController() {
  message_center::MessageCenter::Get()->RemoveObserver(this);
  RemoveModelObserver(this);
}

void AssistantNotificationController::BindRequest(
    mojom::AssistantNotificationControllerRequest request) {
  binding_.Bind(std::move(request));
}

void AssistantNotificationController::AddModelObserver(
    AssistantNotificationModelObserver* observer) {
  model_.AddObserver(observer);
}

void AssistantNotificationController::RemoveModelObserver(
    AssistantNotificationModelObserver* observer) {
  model_.RemoveObserver(observer);
}

void AssistantNotificationController::SetAssistant(
    chromeos::assistant::mojom::Assistant* assistant) {
  assistant_ = assistant;
}

// mojom::AssistantNotificationController --------------------------------------

void AssistantNotificationController::AddOrUpdateNotification(
    AssistantNotificationPtr notification) {
  model_.AddOrUpdateNotification(std::move(notification));
}

void AssistantNotificationController::RemoveNotificationById(
    const std::string& id,
    bool from_server) {
  model_.RemoveNotificationById(id, from_server);
}

void AssistantNotificationController::RemoveNotificationByGroupingKey(
    const std::string& grouping_key,
    bool from_server) {
  model_.RemoveNotificationsByGroupingKey(grouping_key, from_server);
}

void AssistantNotificationController::RemoveAllNotifications(bool from_server) {
  model_.RemoveAllNotifications(from_server);
}

// AssistantNotificationModelObserver ------------------------------------------

void AssistantNotificationController::OnNotificationAdded(
    const AssistantNotification* notification) {
  // Do not show system notifications if the setting is disabled.
  if (!Shell::Get()->voice_interaction_controller()->notification_enabled())
    return;

  message_center::MessageCenter::Get()->AddNotification(
      CreateSystemNotification(notifier_id_, notification));
}

void AssistantNotificationController::OnNotificationUpdated(
    const AssistantNotification* notification) {
  // Do not show system notifications if the setting is disabled.
  if (!Shell::Get()->voice_interaction_controller()->notification_enabled())
    return;

  message_center::MessageCenter::Get()->UpdateNotification(
      notification->client_id,
      CreateSystemNotification(notifier_id_, notification));
}

void AssistantNotificationController::OnNotificationRemoved(
    const AssistantNotification* notification,
    bool from_server) {
  // Remove the notification from the message center.
  message_center::MessageCenter::Get()->RemoveNotification(
      notification->client_id, /*by_user=*/false);

  // Dismiss the notification on the server to sync across devices.
  if (!from_server)
    assistant_->DismissNotification(notification->Clone());
}

void AssistantNotificationController::OnAllNotificationsRemoved(
    bool from_server) {
  message_center::MessageCenter::Get()->RemoveNotificationsForNotifierId(
      notifier_id_);
}

// message_center::MessageCenterObserver ---------------------------------------

void AssistantNotificationController::OnNotificationClicked(
    const std::string& id,
    const base::Optional<int>& button_index,
    const base::Optional<base::string16>& reply) {
  const AssistantNotification* notification = model_.GetNotificationById(id);
  if (!notification)
    return;

  const auto& action_url =
      button_index.has_value()
          ? notification->buttons[button_index.value()]->action_url
          : notification->action_url;

  // Open the action url if it is valid.
  if (IsValidActionUrl(action_url)) {
    assistant_controller_->OpenUrl(action_url);
    model_.RemoveNotificationById(id, /*from_server=*/false);
    return;
  }

  // Otherwise, we retrieve the notification payload from the server using the
  // following indexing scheme:
  //
  // Index:  |    [0]    |   [1]    |   [2]    | ...
  // -------------------------------------------------
  // Action: | Top Level | Button 1 | Button 2 | ...
  const int action_index = button_index.value_or(-1) + 1;
  assistant_->RetrieveNotification(notification->Clone(), action_index);
}

void AssistantNotificationController::OnNotificationRemoved(
    const std::string& notification_id,
    bool by_user) {
  model_.RemoveNotificationById(notification_id, /*from_server=*/false);
}

}  // namespace ash
