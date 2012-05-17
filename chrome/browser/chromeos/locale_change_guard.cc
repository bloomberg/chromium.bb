// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/locale_change_guard.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::UserMetricsAction;
using content::WebContents;

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
  registrar_.Add(this, chrome::NOTIFICATION_OWNERSHIP_CHECKED,
                 content::NotificationService::AllSources());
}

LocaleChangeGuard::~LocaleChangeGuard() {}

void LocaleChangeGuard::OnLogin() {
  registrar_.Add(this, content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
                 content::NotificationService::AllBrowserContextsAndSources());
}

void LocaleChangeGuard::RevertLocaleChange() {
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
  content::RecordAction(UserMetricsAction("LanguageChange_Revert"));
  profile_->ChangeAppLocale(
      from_locale_, Profile::APP_LOCALE_CHANGED_VIA_REVERT);

  Browser* browser = browser::FindTabbedBrowser(profile_, false);
  if (browser)
    browser->ExecuteCommand(IDC_EXIT);
}

void LocaleChangeGuard::RevertLocaleChangeCallback(const ListValue* list) {
  RevertLocaleChange();
}

void LocaleChangeGuard::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  if (profile_ == NULL) {
    NOTREACHED();
    return;
  }
  switch (type) {
    case content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME: {
      if (profile_ ==
          content::Source<WebContents>(source)->GetBrowserContext()) {
        // We need to perform locale change check only once, so unsubscribe.
        registrar_.Remove(this, content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
                          content::NotificationService::AllSources());
        Check();
      }
      break;
    }
    case chrome::NOTIFICATION_OWNERSHIP_CHECKED: {
      if (UserManager::Get()->IsCurrentUserOwner()) {
        PrefService* local_state = g_browser_process->local_state();
        if (local_state) {
          PrefService* prefs = profile_->GetPrefs();
          if (prefs == NULL) {
            NOTREACHED();
            return;
          }
          std::string owner_locale =
              prefs->GetString(prefs::kApplicationLocale);
          if (!owner_locale.empty())
            local_state->SetString(prefs::kOwnerLocale, owner_locale);
        }
      }
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

void LocaleChangeGuard::Check() {
  if (note_ != NULL) {
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
  if (from_locale_ != from_locale || to_locale_ != to_locale) {
    // Falling back to showing message in current locale.
    LOG(ERROR) <<
        "Showing locale change notification in current (not previous) language";
    PrepareChangingLocale(from_locale, to_locale);
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(ash::switches::kAshNotify)) {
    ash::Shell::GetInstance()->tray()->locale_observer()->OnLocaleChanged(
        this, cur_locale, from_locale_, to_locale_);
    return;
  }

  note_.reset(new chromeos::SystemNotification(
      profile_,
      new Delegate(this),
      IDR_NOTIFICATION_LOCALE_CHANGE,
      title_text_));
  note_->Show(
      message_text_, revert_link_text_,
      base::Bind(&LocaleChangeGuard::RevertLocaleChangeCallback,
                 AsWeakPtr()),
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
  content::RecordAction(UserMetricsAction("LanguageChange_Accept"));
  prefs->SetString(prefs::kApplicationLocaleBackup, to_locale_);
  prefs->SetString(prefs::kApplicationLocaleAccepted, to_locale_);
}

void LocaleChangeGuard::PrepareChangingLocale(
    const std::string& from_locale, const std::string& to_locale) {
  std::string cur_locale = g_browser_process->GetApplicationLocale();
  if (!from_locale.empty())
    from_locale_ = from_locale;
  if (!to_locale.empty())
    to_locale_ = to_locale;

  if (!from_locale_.empty() && !to_locale_.empty()) {
    string16 from = l10n_util::GetDisplayNameForLocale(
        from_locale_, cur_locale, true);
    string16 to = l10n_util::GetDisplayNameForLocale(
        to_locale_, cur_locale, true);

    title_text_ = l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_SECTION_TITLE_LANGUAGE);
    message_text_ = l10n_util::GetStringFUTF16(
        IDS_LOCALE_CHANGE_MESSAGE, from, to);
    revert_link_text_ = l10n_util::GetStringFUTF16(
        IDS_LOCALE_CHANGE_REVERT_MESSAGE, from);
  }
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
