// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limit_notifier.h"

#include <memory>

#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

constexpr base::TimeDelta kWarningNotificationTimeout =
    base::TimeDelta::FromMinutes(5);
constexpr base::TimeDelta kExitNotificationTimeout =
    base::TimeDelta::FromMinutes(1);

// The notification id. All the time limit notifications share the same id so
// that a subsequent notification can replace the previous one.
constexpr char kTimeLimitNotificationId[] = "time-limit-notification";

// The notifier id representing the app.
constexpr char kTimeLimitNotifierId[] = "family-link";

void ShowNotification(TimeLimitNotifier::LimitType limit_type,
                      base::TimeDelta time_remaining,
                      content::BrowserContext* context) {
  const base::string16 title = l10n_util::GetStringUTF16(
      limit_type == TimeLimitNotifier::LimitType::kScreenTime
          ? IDS_SCREEN_TIME_NOTIFICATION_TITLE
          : IDS_BED_TIME_NOTIFICATION_TITLE);
  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, kTimeLimitNotificationId,
          title,
          ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                                 ui::TimeFormat::LENGTH_LONG, time_remaining),
          l10n_util::GetStringUTF16(IDS_TIME_LIMIT_NOTIFICATION_DISPLAY_SOURCE),
          GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kTimeLimitNotifierId),
          message_center::RichNotificationData(),
          base::MakeRefCounted<message_center::NotificationDelegate>(),
          ash::kNotificationSupervisedUserIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);
  NotificationDisplayService::GetForProfile(
      Profile::FromBrowserContext(context))
      ->Display(NotificationHandler::Type::TRANSIENT, *notification);
}

}  // namespace

TimeLimitNotifier::TimeLimitNotifier(content::BrowserContext* context)
    : TimeLimitNotifier(context, nullptr /* task_runner */) {}

TimeLimitNotifier::~TimeLimitNotifier() = default;

void TimeLimitNotifier::MaybeScheduleNotifications(
    LimitType limit_type,
    base::TimeDelta remaining_time) {
  // Stop any previously set timers.
  UnscheduleNotifications();

  if (remaining_time >= kWarningNotificationTimeout) {
    warning_notification_timer_.Start(
        FROM_HERE, remaining_time - kWarningNotificationTimeout,
        base::BindOnce(&ShowNotification, limit_type,
                       kWarningNotificationTimeout, context_));
  }
  if (remaining_time >= kExitNotificationTimeout) {
    exit_notification_timer_.Start(
        FROM_HERE, remaining_time - kExitNotificationTimeout,
        base::BindOnce(&ShowNotification, limit_type, kExitNotificationTimeout,
                       context_));
  }
}

void TimeLimitNotifier::UnscheduleNotifications() {
  // TODO(crbug.com/897975): Stop() should be sufficient, but doesn't have the
  // expected effect in tests.
  warning_notification_timer_.AbandonAndStop();
  exit_notification_timer_.AbandonAndStop();
}

TimeLimitNotifier::TimeLimitNotifier(
    content::BrowserContext* context,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : context_(context) {
  if (task_runner.get()) {
    warning_notification_timer_.SetTaskRunner(task_runner);
    exit_notification_timer_.SetTaskRunner(task_runner);
  }
}

}  // namespace chromeos
