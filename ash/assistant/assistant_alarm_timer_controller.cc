// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_alarm_timer_controller.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_notification_controller.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/strings/grit/ash_strings.h"
#include "chromeos/services/assistant/public/features.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

constexpr char kTimerFireNotificationGroupId[] = "assistant/timer_fire";

}  // namespace

AssistantAlarmTimerController::AssistantAlarmTimerController(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller), binding_(this) {}

AssistantAlarmTimerController::~AssistantAlarmTimerController() = default;

void AssistantAlarmTimerController::BindRequest(
    mojom::AssistantAlarmTimerControllerRequest request) {
  DCHECK(chromeos::assistant::features::IsTimerNotificationEnabled());
  binding_.Bind(std::move(request));
}

// TODO(dmblack): Abstract this out into a model that tracks timer state so that
// we can observe it to either add a system notification or an in-Assistant
// notification depending on UI visibility state. These notifications will need
// to be updated in sync with clock ticks until the notification is dismissed.
void AssistantAlarmTimerController::OnTimerSoundingStarted() {
  // TODO(llin): Migrate to use the AlarmManager API to better support multiple
  // timers when the API is available.
  const std::string title =
      l10n_util::GetStringUTF8(IDS_ASSISTANT_TIMER_NOTIFICATION_TITLE);
  const std::string message =
      l10n_util::GetStringUTF8(IDS_ASSISTANT_TIMER_NOTIFICATION_CONTENT);
  const GURL action_url = assistant::util::CreateAssistantQueryDeepLink(
      l10n_util::GetStringUTF8(IDS_ASSISTANT_TIMER_NOTIFICATION_STOP_QUERY));

  chromeos::assistant::mojom::AssistantNotificationPtr notification =
      chromeos::assistant::mojom::AssistantNotification::New();

  notification->title = title;
  notification->message = message;
  notification->action_url = action_url;
  notification->grouping_key = kTimerFireNotificationGroupId;

  // "STOP" button.
  notification->buttons.push_back(
      chromeos::assistant::mojom::AssistantNotificationButton::New(
          l10n_util::GetStringUTF8(
              IDS_ASSISTANT_TIMER_NOTIFICATION_STOP_BUTTON),
          action_url));

  // "ADD 1 MIN" button.
  notification->buttons.push_back(
      chromeos::assistant::mojom::AssistantNotificationButton::New(
          l10n_util::GetStringUTF8(
              IDS_ASSISTANT_TIMER_NOTIFICATION_ADD_1_MIN_BUTTON),
          assistant::util::CreateAssistantQueryDeepLink(
              l10n_util::GetStringUTF8(
                  IDS_ASSISTANT_TIMER_NOTIFICATION_ADD_1_MIN_QUERY))));

  assistant_controller_->notification_controller()->OnShowNotification(
      std::move(notification));
}

void AssistantAlarmTimerController::OnTimerSoundingFinished() {
  assistant_controller_->notification_controller()->OnRemoveNotification(
      kTimerFireNotificationGroupId);
}

}  // namespace ash
