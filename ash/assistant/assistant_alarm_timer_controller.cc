// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_alarm_timer_controller.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_notification_controller.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/i18n/message_formatter.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/services/assistant/public/features.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

// Grouping key for timer notifications.
constexpr char kTimerNotificationGroupingKey[] = "assistant/timer";

// Interval at which alarms/timers are ticked.
constexpr base::TimeDelta kTickInterval = base::TimeDelta::FromSeconds(1);

// Helpers ---------------------------------------------------------------------

std::string CreateTimerNotificationMessage(const AlarmTimer& alarm_timer,
                                           base::TimeDelta time_remaining) {
  const int minutes_remaining = time_remaining.InMinutes();
  const int seconds_remaining =
      (time_remaining - base::TimeDelta::FromMinutes(minutes_remaining))
          .InSeconds();
  return base::UTF16ToUTF8(base::i18n::MessageFormatter::FormatWithNumberedArgs(
      l10n_util::GetStringUTF16(IDS_ASSISTANT_TIMER_NOTIFICATION_MESSAGE),
      alarm_timer.expired() ? "-" : "", minutes_remaining, seconds_remaining));
}

// TODO(llin): Migrate to use the AlarmManager API to better support multiple
// timers when the API is available.
chromeos::assistant::mojom::AssistantNotificationPtr CreateTimerNotification(
    const AlarmTimer& alarm_timer,
    base::TimeDelta time_remaining) {
  using chromeos::assistant::mojom::AssistantNotification;
  using chromeos::assistant::mojom::AssistantNotificationButton;
  using chromeos::assistant::mojom::AssistantNotificationPtr;

  const std::string title =
      l10n_util::GetStringUTF8(IDS_ASSISTANT_TIMER_NOTIFICATION_TITLE);
  const std::string message =
      CreateTimerNotificationMessage(alarm_timer, time_remaining);
  const GURL action_url = assistant::util::CreateAssistantQueryDeepLink(
      l10n_util::GetStringUTF8(IDS_ASSISTANT_TIMER_NOTIFICATION_STOP_QUERY));

  AssistantNotificationPtr notification = AssistantNotification::New();

  notification->title = title;
  notification->message = message;
  notification->action_url = action_url;
  notification->client_id = alarm_timer.id;
  notification->grouping_key = kTimerNotificationGroupingKey;

  // "STOP" button.
  notification->buttons.push_back(AssistantNotificationButton::New(
      l10n_util::GetStringUTF8(IDS_ASSISTANT_TIMER_NOTIFICATION_STOP_BUTTON),
      action_url));

  // "ADD 1 MIN" button.
  notification->buttons.push_back(
      chromeos::assistant::mojom::AssistantNotificationButton::New(
          l10n_util::GetStringUTF8(
              IDS_ASSISTANT_TIMER_NOTIFICATION_ADD_1_MIN_BUTTON),
          assistant::util::CreateAssistantQueryDeepLink(
              l10n_util::GetStringUTF8(
                  IDS_ASSISTANT_TIMER_NOTIFICATION_ADD_1_MIN_QUERY))));

  return notification;
}

}  // namespace

// AssistantAlarmTimerController -----------------------------------------------

AssistantAlarmTimerController::AssistantAlarmTimerController(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller), binding_(this) {
  AddModelObserver(this);
}

AssistantAlarmTimerController::~AssistantAlarmTimerController() {
  RemoveModelObserver(this);
}

void AssistantAlarmTimerController::BindRequest(
    mojom::AssistantAlarmTimerControllerRequest request) {
  DCHECK(chromeos::assistant::features::IsTimerNotificationEnabled());
  binding_.Bind(std::move(request));
}

void AssistantAlarmTimerController::AddModelObserver(
    AssistantAlarmTimerModelObserver* observer) {
  model_.AddObserver(observer);
}

void AssistantAlarmTimerController::RemoveModelObserver(
    AssistantAlarmTimerModelObserver* observer) {
  model_.RemoveObserver(observer);
}

// TODO(dmblack): Remove method when the LibAssistant Alarm/Timer API is ready.
void AssistantAlarmTimerController::OnTimerSoundingStarted() {
  static constexpr char kIdPrefix[] = "assistant/timer";

  AlarmTimer timer;
  timer.id = kIdPrefix + std::to_string(next_timer_id_++);
  timer.type = AlarmTimerType::kTimer;
  timer.end_time = base::TimeTicks::Now();
  model_.AddAlarmTimer(timer);
}

// TODO(dmblack): Remove method when the LibAssistant Alarm/Timer API is ready.
void AssistantAlarmTimerController::OnTimerSoundingFinished() {
  model_.RemoveAllAlarmsTimers();
}

void AssistantAlarmTimerController::OnAlarmTimerAdded(
    const AlarmTimer& alarm_timer,
    const base::TimeDelta& time_remaining) {
  // Schedule a repeating timer to tick the tracked alarms/timers.
  if (chromeos::assistant::features::IsTimerTicksEnabled() &&
      !timer_.IsRunning()) {
    timer_.Start(FROM_HERE, kTickInterval, &model_,
                 &AssistantAlarmTimerModel::Tick);
  }

  // Create a notification for the added alarm/timer.
  DCHECK(chromeos::assistant::features::IsTimerNotificationEnabled());
  if (chromeos::assistant::features::IsTimerNotificationEnabled()) {
    assistant_controller_->notification_controller()->AddOrUpdateNotification(
        CreateTimerNotification(alarm_timer, time_remaining));
  }
}

void AssistantAlarmTimerController::OnAlarmsTimersTicked(
    const std::map<std::string, base::TimeDelta>& times_remaining) {
  // This code should only be called when timer notifications/ticks are enabled.
  DCHECK(chromeos::assistant::features::IsTimerNotificationEnabled());
  DCHECK(chromeos::assistant::features::IsTimerTicksEnabled());

  // Update any existing notifications associated w/ our alarms/timers.
  for (auto& pair : times_remaining) {
    auto* notification_controller =
        assistant_controller_->notification_controller();
    if (notification_controller->model()->HasNotificationForId(pair.first)) {
      notification_controller->AddOrUpdateNotification(CreateTimerNotification(
          *model_.GetAlarmTimerById(pair.first), pair.second));
    }
  }
}

void AssistantAlarmTimerController::OnAllAlarmsTimersRemoved() {
  if (chromeos::assistant::features::IsTimerTicksEnabled())
    timer_.Stop();

  // Remove any notifications associated w/ alarms/timers.
  DCHECK(chromeos::assistant::features::IsTimerNotificationEnabled());
  if (chromeos::assistant::features::IsTimerNotificationEnabled()) {
    assistant_controller_->notification_controller()
        ->RemoveNotificationByGroupingKey(kTimerNotificationGroupingKey,
                                          /*from_server=*/false);
  }
}

}  // namespace ash
