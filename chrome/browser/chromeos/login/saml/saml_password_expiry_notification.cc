// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/saml/saml_password_expiry_notification.h"

#include "ash/public/cpp/notification_utils.h"
#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/insession_password_change_ui.h"
#include "chrome/common/pref_names.h"
#include "chromeos/login/auth/saml_password_attributes.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

using message_center::HandleNotificationClickDelegate;
using message_center::Notification;
using message_center::NotificationType;
using message_center::NotifierId;
using message_center::NotifierType;
using message_center::RichNotificationData;
using message_center::SystemNotificationWarningLevel;

namespace chromeos {

namespace {

// Unique ID for this notification.
const char kNotificationId[] = "saml.password-expiry-notification";

// NotifierId for histogram reporting.
const base::NoDestructor<NotifierId> kNotifierId(
    message_center::NotifierType::SYSTEM_COMPONENT,
    kNotificationId);

// Simplest type of notification - has text but no other UI elements.
const NotificationType kNotificationType =
    message_center::NOTIFICATION_TYPE_SIMPLE;

// Generic type for notifications that are not from web pages etc.
const NotificationHandler::Type kNotificationHandlerType =
    NotificationHandler::Type::TRANSIENT;

// The icon to use for this notification - looks like an office building.
const gfx::VectorIcon& kIcon = vector_icons::kBusinessIcon;

// Warning level NORMAL means the notification heading is blue.
const SystemNotificationWarningLevel kWarningLevel =
    SystemNotificationWarningLevel::NORMAL;

// Leaving this empty means the notification is attributed to the system -
// ie "Chromium OS" or similar.
const base::NoDestructor<base::string16> kDisplaySource;

// These extra attributes aren't needed, so they are left empty.
const base::NoDestructor<GURL> kOriginUrl;
const base::NoDestructor<RichNotificationData> kRichNotificationData;

// Line separator in the notification body.
const base::NoDestructor<base::string16> kLineSeparator(
    base::string16(1, '\n'));

// Callback called when notification is clicked - opens password-change page.
void OnNotificationClicked() {
  PasswordChangeDialog::Show();
}

const base::NoDestructor<scoped_refptr<HandleNotificationClickDelegate>>
    kClickDelegate(base::MakeRefCounted<HandleNotificationClickDelegate>(
        base::BindRepeating(&OnNotificationClicked)));

base::string16 GetTitleText(int lessThanNDays) {
  const bool hasExpired = (lessThanNDays <= 0);
  return hasExpired ? l10n_util::GetStringUTF16(IDS_PASSWORD_HAS_EXPIRED_TITLE)
                    : l10n_util::GetStringUTF16(IDS_PASSWORD_WILL_EXPIRE_TITLE);
}

base::string16 GetBodyText(int lessThanNDays) {
  const std::vector<base::string16> body_lines = {
      l10n_util::GetPluralStringFUTF16(IDS_PASSWORD_EXPIRY_DAYS_BODY,
                                       std::max(lessThanNDays, 0)),
      l10n_util::GetStringUTF16(IDS_PASSWORD_EXPIRY_CHOOSE_NEW_PASSWORD_LINK)};

  return base::JoinString(body_lines, *kLineSeparator);
}

// A time delta of length zero.
const base::TimeDelta kZeroTimeDelta = base::TimeDelta();
// A time delta of length one hour.
const base::TimeDelta kOneHour = base::TimeDelta::FromHours(1);

// Traits for running RecheckTask. Runs from the UI thread to show notification.
const base::TaskTraits kRecheckTaskTraits = {
    content::BrowserThread::UI, base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

// A task to check again if the notification should be shown at a later time.
// This has weak pointers so that it pending tasks can be canceled.
class RecheckTask {
 public:
  void Run(Profile* profile) {
    MaybeShowSamlPasswordExpiryNotification(profile);
  }

  // Check again whether to show notification for |profile| after |delay|.
  void PostDelayed(Profile* profile, base::TimeDelta delay) {
    base::PostDelayedTaskWithTraits(
        FROM_HERE, kRecheckTaskTraits,
        base::BindOnce(&RecheckTask::Run, weak_ptr_factory.GetWeakPtr(),
                       profile),
        delay);
  }

  // Cancel any scheduled tasks to check again.
  void CancelIfPending() { weak_ptr_factory.InvalidateWeakPtrs(); }

 private:
  base::WeakPtrFactory<RecheckTask> weak_ptr_factory{this};
};

// Keep only a single instance of the RecheckTask, so that we can easily cancel
// all pending tasks just by invalidating weak pointers to this one task.
base::NoDestructor<RecheckTask> recheck_task_instance;

}  // namespace

void MaybeShowSamlPasswordExpiryNotification(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // This cancels any pending RecheckTask - we don't want a situation where
  // accidentally this function more than once means lots of identical
  // RecheckTasks are added to the work queue.
  recheck_task_instance->CancelIfPending();

  // This could be run after a long delay. Check it still makes sense to run.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager || !profile_manager->IsValidProfile(profile)) {
    return;
  }

  SamlPasswordAttributes attrs =
      SamlPasswordAttributes::LoadFromPrefs(profile->GetPrefs());
  if (!attrs.has_expiration_time()) {
    // No reason to believe the password will ever expire.
    // Hide the notification (just in case it is shown) and return.
    DismissSamlPasswordExpiryNotification(profile);
    return;
  }

  const base::TimeDelta time_until_expiry =
      attrs.expiration_time() - base::Time::Now();
  if (time_until_expiry <= kZeroTimeDelta) {
    // The password has expired, so we show the notification now.
    ShowSamlPasswordExpiryNotification(profile, /*less_than_n_days=*/0);
    return;
  }

  // The password has not expired, but it will in the future.
  const int less_than_n_days = time_until_expiry.InDaysFloored() + 1;
  int advance_warning_days = profile->GetPrefs()->GetInteger(
      prefs::kSamlPasswordExpirationAdvanceWarningDays);
  advance_warning_days = std::max(advance_warning_days, 0);

  if (less_than_n_days <= advance_warning_days) {
    // The password will expire in less than |advance_warning_days|, so we show
    // a notification now explaining the password will expire soon.
    ShowSamlPasswordExpiryNotification(profile, less_than_n_days);
    return;
  }

  // We have not even reached the advance warning threshold. Run this code again
  // once we have arrived at expiry_time minus advance_warning_days...
  base::TimeDelta recheck_delay =
      time_until_expiry - base::TimeDelta::FromDays(advance_warning_days);
  // But, wait an extra hour so that when this code is next run, it is clear we
  // are now inside advance_warning_days (and not right on the boundary).
  recheck_delay += kOneHour;
  // And never wait less than an hour before running again - we don't want some
  // bug causing this code to run every millisecond...
  recheck_delay = std::max(recheck_delay, kOneHour);

  // Check again whether to show notification after recheck_delay.
  recheck_task_instance->PostDelayed(profile, recheck_delay);
}

void ShowSamlPasswordExpiryNotification(Profile* profile, int lessThanNDays) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::string16 title = GetTitleText(lessThanNDays);
  const base::string16 body = GetBodyText(lessThanNDays);

  std::unique_ptr<Notification> notification = ash::CreateSystemNotification(
      kNotificationType, kNotificationId, title, body, *kDisplaySource,
      *kOriginUrl, *kNotifierId, *kRichNotificationData, *kClickDelegate, kIcon,
      kWarningLevel);

  NotificationDisplayServiceFactory::GetForProfile(profile)->Display(
      kNotificationHandlerType, *notification, /*metadata=*/nullptr);
}

void DismissSamlPasswordExpiryNotification(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  NotificationDisplayServiceFactory::GetForProfile(profile)->Close(
      kNotificationHandlerType, kNotificationId);
}

}  // namespace chromeos
