// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_notification_controller.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/new_window_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr char kNotificationId[] = "assistant";
constexpr char kNotifierAssistant[] = "assistant";

}  // namespace

AssistantNotificationController::AssistantNotificationController(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller),
      assistant_notification_subscriber_binding_(this) {}

AssistantNotificationController::~AssistantNotificationController() = default;

void AssistantNotificationController::SetAssistant(
    chromeos::assistant::mojom::Assistant* assistant) {
  assistant_ = assistant;

  // Subscribe to Assistant notification events.
  chromeos::assistant::mojom::AssistantNotificationSubscriberPtr ptr;
  assistant_notification_subscriber_binding_.Bind(mojo::MakeRequest(&ptr));
  assistant_->AddAssistantNotificationSubscriber(std::move(ptr));
}

void AssistantNotificationController::OnShowNotification(
    AssistantNotificationPtr notification) {
  DCHECK(assistant_);

  // Create the specified |notification| that should be rendered in the
  // |message_center| for the interaction.
  notification_ = std::move(notification);
  const base::string16 title = base::UTF8ToUTF16(notification_->title);
  const base::string16 message = base::UTF8ToUTF16(notification_->message);
  const GURL action_url = notification_->action_url;
  const base::string16 display_source =
      l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_NOTIFICATION_DISPLAY_SOURCE);

  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  message_center::RichNotificationData optional_field;

  // TODO(wutao): Handle invalid |action_url|, register different click handler,
  // etc.
  std::unique_ptr<message_center::Notification> system_notification =
      message_center::Notification::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId, title,
          message, gfx::Image(), display_source, action_url,
          message_center::NotifierId(
              message_center::NotifierId::SYSTEM_COMPONENT, kNotifierAssistant),
          optional_field,
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(
                  [](base::WeakPtr<AssistantController> assistant_controller,
                     const GURL& action_url) {
                    if (assistant_controller)
                      assistant_controller->OpenUrl(action_url);
                  },
                  assistant_controller_->GetWeakPtr(), action_url)),
          kAssistantIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);
  system_notification->set_priority(message_center::DEFAULT_PRIORITY);
  message_center->AddNotification(std::move(system_notification));
}

void AssistantNotificationController::OnRemoveNotification(
    const std::string& grouping_key) {
  if (!grouping_key.empty() &&
      (!notification_ || notification_->grouping_key != grouping_key)) {
    return;
  }

  // message_center has only one Assistant notification, so there is no
  // difference between removing all and removing one Assistant notification.
  notification_.reset();
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  message_center->RemoveNotification(kNotificationId, /*by_user=*/false);
}

}  // namespace ash
