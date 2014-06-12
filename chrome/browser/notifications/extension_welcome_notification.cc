// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/extension_welcome_notification.h"

#include "base/guid.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"
#include "ui/message_center/notification_types.h"

const int ExtensionWelcomeNotification::kRequestedShowTimeDays = 14;

namespace {

class NotificationCallbacks
    : public message_center::NotificationDelegate {
 public:
  NotificationCallbacks(
      Profile* profile,
      const message_center::NotifierId notifier_id,
      const std::string& welcome_notification_id,
      ExtensionWelcomeNotification::Delegate* delegate)
      : profile_(profile),
        notifier_id_(notifier_id.type, notifier_id.id),
        welcome_notification_id_(welcome_notification_id),
        delegate_(delegate) {
  }

  // Overridden from NotificationDelegate:
  virtual void Display() OVERRIDE {}
  virtual void Error() OVERRIDE {}

  virtual void Close(bool by_user) OVERRIDE {
    if (by_user) {
      // Setting the preference here may cause the notification erasing
      // to reenter. Posting a task avoids this issue.
      delegate_->PostTask(
          FROM_HERE,
          base::Bind(&NotificationCallbacks::MarkAsDismissed, this));
    }
  }

  virtual void Click() OVERRIDE {}
  virtual void ButtonClick(int index) OVERRIDE {
    if (index == 0) {
      OpenNotificationLearnMoreTab();
    } else if (index == 1) {
      DisableNotificationProvider();
      Close(true);
    } else {
      NOTREACHED();
    }
  }

 private:
  void MarkAsDismissed() {
    profile_->GetPrefs()->SetBoolean(prefs::kWelcomeNotificationDismissedLocal,
                                     true);
  }

  void OpenNotificationLearnMoreTab() {
    chrome::NavigateParams params(
        profile_,
        GURL(chrome::kNotificationWelcomeLearnMoreURL),
        content::PAGE_TRANSITION_LINK);
    params.disposition = NEW_FOREGROUND_TAB;
    params.window_action = chrome::NavigateParams::SHOW_WINDOW;
    chrome::Navigate(&params);
  }

  void DisableNotificationProvider() {
    message_center::Notifier notifier(notifier_id_, base::string16(), true);
    message_center::MessageCenter* message_center =
        delegate_->GetMessageCenter();
    message_center->DisableNotificationsByNotifier(notifier_id_);
    message_center->RemoveNotification(welcome_notification_id_, false);
    message_center->GetNotifierSettingsProvider()->SetNotifierEnabled(
        notifier, false);
  }

  virtual ~NotificationCallbacks() {}

  Profile* const profile_;

  const message_center::NotifierId notifier_id_;

  std::string welcome_notification_id_;

  // Weak ref owned by ExtensionWelcomeNotification.
  ExtensionWelcomeNotification::Delegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(NotificationCallbacks);
};

class DefaultDelegate : public ExtensionWelcomeNotification::Delegate {
 public:
  DefaultDelegate() {}

  virtual message_center::MessageCenter* GetMessageCenter() OVERRIDE {
    return g_browser_process->message_center();
  }

  virtual base::Time GetCurrentTime() OVERRIDE {
    return base::Time::Now();
  }

  virtual void PostTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task) OVERRIDE {
    base::MessageLoop::current()->PostTask(from_here, task);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultDelegate);
};

}  // namespace

ExtensionWelcomeNotification::ExtensionWelcomeNotification(
    const std::string& extension_id,
    Profile* const profile,
    ExtensionWelcomeNotification::Delegate* const delegate)
    : notifier_id_(message_center::NotifierId::APPLICATION, extension_id),
      profile_(profile),
      delegate_(delegate) {
  welcome_notification_dismissed_pref_.Init(
      prefs::kWelcomeNotificationDismissed,
      profile_->GetPrefs(),
      base::Bind(
          &ExtensionWelcomeNotification::OnWelcomeNotificationDismissedChanged,
          base::Unretained(this)));
  welcome_notification_dismissed_local_pref_.Init(
      prefs::kWelcomeNotificationDismissedLocal,
      profile_->GetPrefs());
}

// static
scoped_ptr<ExtensionWelcomeNotification> ExtensionWelcomeNotification::Create(
    const std::string& extension_id,
    Profile* const profile) {
  return Create(extension_id, profile, new DefaultDelegate()).Pass();
}

// static
scoped_ptr<ExtensionWelcomeNotification> ExtensionWelcomeNotification::Create(
    const std::string& extension_id,
    Profile* const profile,
    Delegate* const delegate) {
  return scoped_ptr<ExtensionWelcomeNotification>(
      new ExtensionWelcomeNotification(extension_id, profile, delegate)).Pass();
}

ExtensionWelcomeNotification::~ExtensionWelcomeNotification() {
  if (delayed_notification_) {
    delayed_notification_.reset();
    PrefServiceSyncable::FromProfile(profile_)->RemoveObserver(this);
  } else {
    HideWelcomeNotification();
  }
}

void ExtensionWelcomeNotification::OnIsSyncingChanged() {
  DCHECK(delayed_notification_);
  PrefServiceSyncable* const pref_service_syncable =
      PrefServiceSyncable::FromProfile(profile_);
  if (pref_service_syncable->IsSyncing()) {
    pref_service_syncable->RemoveObserver(this);
    scoped_ptr<Notification> previous_notification(
        delayed_notification_.release());
    ShowWelcomeNotificationIfNecessary(*(previous_notification.get()));
  }
}

void ExtensionWelcomeNotification::ShowWelcomeNotificationIfNecessary(
    const Notification& notification) {
  if ((notification.notifier_id() == notifier_id_) && !delayed_notification_) {
    PrefServiceSyncable* const pref_service_syncable =
        PrefServiceSyncable::FromProfile(profile_);
    if (pref_service_syncable->IsSyncing()) {
      PrefService* const pref_service = profile_->GetPrefs();
      if (!UserHasDismissedWelcomeNotification()) {
        const PopUpRequest pop_up_request =
            pref_service->GetBoolean(
                prefs::kWelcomeNotificationPreviouslyPoppedUp)
                ? POP_UP_HIDDEN
                : POP_UP_SHOWN;
        if (pop_up_request == POP_UP_SHOWN) {
          pref_service->SetBoolean(
              prefs::kWelcomeNotificationPreviouslyPoppedUp, true);
        }

        if (IsWelcomeNotificationExpired()) {
          ExpireWelcomeNotification();
        } else {
          ShowWelcomeNotification(
              notification.display_source(), pop_up_request);
        }
      }
    } else {
      delayed_notification_.reset(new Notification(notification));
      pref_service_syncable->AddObserver(this);
    }
  }
}

// static
void ExtensionWelcomeNotification::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* prefs) {
  prefs->RegisterBooleanPref(prefs::kWelcomeNotificationDismissed,
                             false,
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWelcomeNotificationDismissedLocal,
                             false,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWelcomeNotificationPreviouslyPoppedUp,
                             false,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  prefs->RegisterInt64Pref(prefs::kWelcomeNotificationExpirationTimestamp,
                           0,
                           user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

message_center::MessageCenter*
ExtensionWelcomeNotification::GetMessageCenter() const {
  return delegate_->GetMessageCenter();
}

void ExtensionWelcomeNotification::ShowWelcomeNotification(
    const base::string16& display_source,
    const PopUpRequest pop_up_request) {
  message_center::ButtonInfo learn_more(
      l10n_util::GetStringUTF16(IDS_NOTIFICATION_WELCOME_BUTTON_LEARN_MORE));
  learn_more.icon = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_NOTIFICATION_WELCOME_LEARN_MORE);
  message_center::ButtonInfo disable(
      l10n_util::GetStringUTF16(IDS_NOTIFIER_WELCOME_BUTTON));
  disable.icon = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_NOTIFIER_BLOCK_BUTTON);

  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.priority = 2;
  rich_notification_data.buttons.push_back(learn_more);
  rich_notification_data.buttons.push_back(disable);

  if (welcome_notification_id_.empty())
    welcome_notification_id_ = base::GenerateGUID();

  if (!welcome_notification_id_.empty()) {
    scoped_ptr<message_center::Notification> message_center_notification(
        new message_center::Notification(
            message_center::NOTIFICATION_TYPE_BASE_FORMAT,
            welcome_notification_id_,
            l10n_util::GetStringUTF16(IDS_NOTIFICATION_WELCOME_TITLE),
            l10n_util::GetStringUTF16(IDS_NOTIFICATION_WELCOME_BODY),
            ui::ResourceBundle::GetSharedInstance().GetImageNamed(
                IDR_NOTIFICATION_WELCOME_ICON),
            display_source,
            notifier_id_,
            rich_notification_data,
            new NotificationCallbacks(
                profile_, notifier_id_, welcome_notification_id_,
                delegate_.get())));

    if (pop_up_request == POP_UP_HIDDEN)
      message_center_notification->set_shown_as_popup(true);

    GetMessageCenter()->AddNotification(message_center_notification.Pass());
    StartExpirationTimer();
  }
}

void ExtensionWelcomeNotification::HideWelcomeNotification() {
  if (!welcome_notification_id_.empty() &&
      GetMessageCenter()->FindVisibleNotificationById(
          welcome_notification_id_) != NULL) {
    GetMessageCenter()->RemoveNotification(welcome_notification_id_, false);
    StopExpirationTimer();
  }
}

bool ExtensionWelcomeNotification::UserHasDismissedWelcomeNotification() const {
  // This was previously a syncable preference; now it's per-machine.
  // Only the local pref will be written moving forward, but check for both so
  // users won't be double-toasted.
  bool shown_synced = profile_->GetPrefs()->GetBoolean(
      prefs::kWelcomeNotificationDismissed);
  bool shown_local = profile_->GetPrefs()->GetBoolean(
      prefs::kWelcomeNotificationDismissedLocal);
  return (shown_synced || shown_local);
}

void ExtensionWelcomeNotification::OnWelcomeNotificationDismissedChanged() {
  if (UserHasDismissedWelcomeNotification()) {
    HideWelcomeNotification();
  }
}

void ExtensionWelcomeNotification::StartExpirationTimer() {
  if (!expiration_timer_ && !IsWelcomeNotificationExpired()) {
    base::Time expiration_timestamp = GetExpirationTimestamp();
    if (expiration_timestamp.is_null()) {
      SetExpirationTimestampFromNow();
      expiration_timestamp = GetExpirationTimestamp();
      DCHECK(!expiration_timestamp.is_null());
    }
    expiration_timer_.reset(
        new base::OneShotTimer<ExtensionWelcomeNotification>());
    expiration_timer_->Start(
        FROM_HERE,
        expiration_timestamp - delegate_->GetCurrentTime(),
        this,
        &ExtensionWelcomeNotification::ExpireWelcomeNotification);
  }
}

void ExtensionWelcomeNotification::StopExpirationTimer() {
  if (expiration_timer_) {
    expiration_timer_->Stop();
    expiration_timer_.reset();
  }
}

void ExtensionWelcomeNotification::ExpireWelcomeNotification() {
  DCHECK(IsWelcomeNotificationExpired());
  profile_->GetPrefs()->SetBoolean(
      prefs::kWelcomeNotificationDismissedLocal, true);
  HideWelcomeNotification();
}

base::Time ExtensionWelcomeNotification::GetExpirationTimestamp() const {
  PrefService* const pref_service = profile_->GetPrefs();
  const int64 expiration_timestamp =
      pref_service->GetInt64(prefs::kWelcomeNotificationExpirationTimestamp);
  return (expiration_timestamp == 0)
      ? base::Time()
      : base::Time::FromInternalValue(expiration_timestamp);
}

void ExtensionWelcomeNotification::SetExpirationTimestampFromNow() {
  PrefService* const pref_service = profile_->GetPrefs();
  pref_service->SetInt64(
      prefs::kWelcomeNotificationExpirationTimestamp,
      (delegate_->GetCurrentTime() +
          base::TimeDelta::FromDays(kRequestedShowTimeDays)).ToInternalValue());
}

bool ExtensionWelcomeNotification::IsWelcomeNotificationExpired() const {
  const base::Time expiration_timestamp = GetExpirationTimestamp();
  return !expiration_timestamp.is_null() &&
         (expiration_timestamp <= delegate_->GetCurrentTime());
}
