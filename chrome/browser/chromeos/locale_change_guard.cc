// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/locale_change_guard.h"

#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "content/common/notification_source.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

class LocaleChangeGuard::Delegate : public NotificationDelegate {
 public:
  explicit Delegate(chromeos::LocaleChangeGuard* master) : master_(master) {}
  void Close(bool by_user);
  void Display() {}
  void Error() {}
  void Click() {}
  std::string id() const;

 private:
  chromeos::LocaleChangeGuard* master_;

  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

LocaleChangeGuard::LocaleChangeGuard(Profile* profile)
    : profile_(profile),
      note_(NULL),
      reverted_(false) {
  DCHECK(profile_);
  registrar_.Add(this, NotificationType::LOAD_COMPLETED_MAIN_FRAME,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::OWNERSHIP_CHECKED,
                 NotificationService::AllSources());
}

void LocaleChangeGuard::RevertLocaleChange(const ListValue* list) {
  if (note_ == NULL ||
      profile_ == NULL ||
      from_locale_.empty() ||
      to_locale_.empty()) {
    NOTREACHED();
    return;
  }
  if (reverted_)
    return;
  reverted_ = true;
  UserMetrics::RecordAction(UserMetricsAction("LanguageChange_Revert"));
  profile_->ChangeAppLocale(
      from_locale_, Profile::APP_LOCALE_CHANGED_VIA_REVERT);

  Browser* browser = Browser::GetTabbedBrowser(profile_, false);
  if (browser)
    browser->ExecuteCommand(IDC_EXIT);
}

void LocaleChangeGuard::Observe(NotificationType type,
                                const NotificationSource& source,
                                const NotificationDetails& details) {
  if (profile_ == NULL) {
    NOTREACHED();
    return;
  }
  switch (type.value) {
    case NotificationType::LOAD_COMPLETED_MAIN_FRAME:
      // We need to perform locale change check only once, so unsubscribe.
      registrar_.Remove(this, NotificationType::LOAD_COMPLETED_MAIN_FRAME,
                        NotificationService::AllSources());
      Check();
      break;
    case NotificationType::OWNERSHIP_CHECKED:
      if (UserManager::Get()->current_user_is_owner()) {
        PrefService* local_state = g_browser_process->local_state();
        if (local_state) {
          PrefService* prefs = profile_->GetPrefs();
          if (prefs == NULL) {
            NOTREACHED();
            return;
          }
          std::string owner_locale =
              prefs->GetString(prefs::kApplicationLocale);
          if (!owner_locale.empty()) {
            local_state->SetString(prefs::kOwnerLocale, owner_locale);
            local_state->ScheduleSavePersistentPrefs();
          }
        }
      }
      break;
    default:
      NOTREACHED();
      break;
  }
}

void LocaleChangeGuard::Check() {
  if (note_ != NULL || !from_locale_.empty() || !to_locale_.empty()) {
    // Somehow we are invoked more than once. Once is enough.
    return;
  }

  std::string cur_locale = g_browser_process->GetApplicationLocale();
  if (cur_locale.empty()) {
    NOTREACHED();
    return;
  }

  PrefService* prefs = profile_->GetPrefs();
  if (prefs == NULL) {
    NOTREACHED();
    return;
  }

  std::string to_locale = prefs->GetString(prefs::kApplicationLocale);
  if (to_locale != cur_locale) {
    // This conditional branch can occur in cases like:
    // (1) kApplicationLocale preference was modified by synchronization;
    // (2) kApplicationLocale is managed by policy.
    return;
  }

  std::string from_locale = prefs->GetString(prefs::kApplicationLocaleBackup);
  if (from_locale.empty() || from_locale == to_locale)
    return;  // No locale change was detected, just exit.

  if (prefs->GetString(prefs::kApplicationLocaleAccepted) == to_locale)
    return;  // Already accepted.

  // Locale change detected, showing notification.
  from_locale_ = from_locale;
  to_locale_ = to_locale;
  note_.reset(new chromeos::SystemNotification(
      profile_,
      new Delegate(this),
      IDR_DEFAULT_FAVICON,
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_SECTION_TITLE_LANGUAGE)));
  note_->Show(
      l10n_util::GetStringFUTF16(
          IDS_LOCALE_CHANGE_MESSAGE,
          l10n_util::GetDisplayNameForLocale(from_locale_, to_locale_, true),
          l10n_util::GetDisplayNameForLocale(to_locale_, to_locale_, true)),
      l10n_util::GetStringUTF16(IDS_LOCALE_CHANGE_REVERT_MESSAGE),
      NewCallback(this, &LocaleChangeGuard::RevertLocaleChange),
      true,  // urgent
      false);  // non-sticky
}

void LocaleChangeGuard::AcceptLocaleChange() {
  if (note_ == NULL ||
      profile_ == NULL ||
      from_locale_.empty() ||
      to_locale_.empty()) {
    NOTREACHED();
    return;
  }

  // Check whether locale has been reverted or changed.
  // If not: mark current locale as accepted.
  if (reverted_)
    return;
  PrefService* prefs = profile_->GetPrefs();
  if (prefs == NULL) {
    NOTREACHED();
    return;
  }
  if (prefs->GetString(prefs::kApplicationLocale) != to_locale_)
    return;
  UserMetrics::RecordAction(UserMetricsAction("LanguageChange_Accept"));
  prefs->SetString(prefs::kApplicationLocaleBackup, to_locale_);
  prefs->SetString(prefs::kApplicationLocaleAccepted, to_locale_);
  prefs->ScheduleSavePersistentPrefs();
}

void LocaleChangeGuard::Delegate::Close(bool by_user) {
  if (by_user)
    master_->AcceptLocaleChange();
}

std::string LocaleChangeGuard::Delegate::id() const {
  // Arbitrary unique Id.
  return "8c386938-1e3f-11e0-ac7b-18a90520e2e5";
}

}  // namespace chromeos
