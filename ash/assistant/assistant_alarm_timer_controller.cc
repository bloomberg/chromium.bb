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

// Grouping key for timer notifications.
constexpr char kTimerNotificationGroupingKey[] = "assistant/timer";

// Interval at which alarms/timers are ticked.
constexpr base::TimeDelta kTickInterval = base::TimeDelta::FromSeconds(1);

}  // namespace

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
  if (!timer_.IsRunning()) {
    timer_.Start(FROM_HERE, kTickInterval, &model_,
                 &AssistantAlarmTimerModel::Tick);
  }

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
  notification->client_id = alarm_timer.id;
  notification->grouping_key = kTimerNotificationGroupingKey;

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

  DCHECK(chromeos::assistant::features::IsTimerNotificationEnabled());
  assistant_controller_->notification_controller()->AddNotification(
      std::move(notification));
}

void AssistantAlarmTimerController::OnAllAlarmsTimersRemoved() {
  timer_.Stop();

  DCHECK(chromeos::assistant::features::IsTimerNotificationEnabled());
  assistant_controller_->notification_controller()
      ->RemoveNotificationByGroupingKey(kTimerNotificationGroupingKey,
                                        /*from_server=*/false);
}

}  // namespace ash
