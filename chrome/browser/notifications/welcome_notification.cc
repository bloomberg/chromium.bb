// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/welcome_notification.h"

#include "base/guid.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"
#include "ui/message_center/notification_types.h"

const char kChromeNowExtensionID[] = "pafkbggdmjlpgkdkcbjmhmfcdpncadgh";

class WelcomeNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  WelcomeNotificationDelegate(const std::string& id, Profile* profile)
      : profile_(profile) {}

  // Overridden from NotificationDelegate:
  virtual void Display() OVERRIDE {}
  virtual void Error() OVERRIDE {}

  virtual void Close(bool by_user) OVERRIDE {
    if (by_user) {
      // Setting the preference here may cause the notification erasing
      // to reenter. Posting a task avoids this issue.
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&WelcomeNotificationDelegate::MarkAsDismissed, this));
    }
  }

  virtual void Click() OVERRIDE {}
  virtual void ButtonClick(int index) OVERRIDE {
    DCHECK(index == 0);
    OpenNotificationLearnMoreTab();
  }

 private:
  void MarkAsDismissed() {
    profile_->GetPrefs()->
        SetBoolean(prefs::kWelcomeNotificationDismissed, true);
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

  virtual ~WelcomeNotificationDelegate() {}

  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(WelcomeNotificationDelegate);
};

WelcomeNotification::WelcomeNotification(
    Profile* profile,
    message_center::MessageCenter* message_center)
    : profile_(profile),
      message_center_(message_center) {
  welcome_notification_dismissed_pref_.Init(
    prefs::kWelcomeNotificationDismissed,
    profile_->GetPrefs(),
    base::Bind(
        &WelcomeNotification::OnWelcomeNotificationDismissedChanged,
        base::Unretained(this)));
}

WelcomeNotification::~WelcomeNotification() {}

void WelcomeNotification::ShowWelcomeNotificationIfNecessary(
    const Notification& notification) {
  message_center::NotifierId notifier_id = notification.notifier_id();
  if (notifier_id.id == kChromeNowExtensionID) {
    PrefService* pref_service = profile_->GetPrefs();
    if (!pref_service->GetBoolean(prefs::kWelcomeNotificationDismissed)) {
      PopUpRequest pop_up_request =
          pref_service->GetBoolean(
              prefs::kWelcomeNotificationPreviouslyPoppedUp)
          ? POP_UP_HIDDEN
          : POP_UP_SHOWN;
      if (pop_up_request == POP_UP_SHOWN) {
        pref_service->SetBoolean(
            prefs::kWelcomeNotificationPreviouslyPoppedUp, true);
      }

      ShowWelcomeNotification(notifier_id,
                              notification.display_source(),
                              pop_up_request);
    }
  }
}

// Static
void WelcomeNotification::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* prefs) {
  prefs->RegisterBooleanPref(
      prefs::kWelcomeNotificationDismissed,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(
      prefs::kWelcomeNotificationPreviouslyPoppedUp,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

void WelcomeNotification::ShowWelcomeNotification(
    const message_center::NotifierId notifier_id,
    const string16& display_source,
    PopUpRequest pop_up_request) {
  message_center::ButtonInfo learn_more(
      l10n_util::GetStringUTF16(IDS_NOTIFICATION_WELCOME_BUTTON_LEARN_MORE));
  learn_more.icon = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_NOTIFICATION_WELCOME_LEARN_MORE);

  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.priority = 2;
  rich_notification_data.buttons.push_back(learn_more);

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
            notifier_id,
            rich_notification_data,
            new WelcomeNotificationDelegate(
                welcome_notification_id_, profile_)));

    if (pop_up_request == POP_UP_HIDDEN)
      message_center_notification->set_shown_as_popup(true);

    message_center_->AddNotification(message_center_notification.Pass());
  }
}

void WelcomeNotification::HideWelcomeNotification() {
  if (!welcome_notification_id_.empty() &&
      message_center_->HasNotification(welcome_notification_id_)) {
    message_center_->RemoveNotification(welcome_notification_id_, false);
  }
}

void WelcomeNotification::OnWelcomeNotificationDismissedChanged() {
  const bool welcome_notification_dismissed =
      profile_->GetPrefs()->GetBoolean(prefs::kWelcomeNotificationDismissed);
  if (welcome_notification_dismissed)
    HideWelcomeNotification();
}
