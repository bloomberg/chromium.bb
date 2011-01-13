// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/locale_change_guard.h"

#include "app/l10n_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"

namespace {

base::LazyInstance<chromeos::LocaleChangeGuard> g_locale_change_guard(
    base::LINKER_INITIALIZED);

}  // namespace

namespace chromeos {

LocaleChangeGuard::LocaleChangeGuard()
    : profile_id_(Profile::InvalidProfileId),
      tab_contents_(NULL),
      note_(NULL),
      reverted_(false) {
}

void LocaleChangeGuard::RevertLocaleChange(const ListValue* list) {
  reverted_ = true;
  UserMetrics::RecordAction(UserMetricsAction("LanguageChange_Revert"));
  tab_contents_->profile()->ChangeApplicationLocale(from_locale_, true);
  PrefService* prefs = tab_contents_->profile()->GetPrefs();
  if (prefs) {
    prefs->SetString(prefs::kApplicationLocaleBackup, from_locale_);
    prefs->ClearPref(prefs::kApplicationLocaleAccepted);
    prefs->ScheduleSavePersistentPrefs();
  }
  Browser* browser = Browser::GetBrowserForController(
      &tab_contents_->controller(), NULL);
  if (browser)
    browser->ExecuteCommand(IDC_EXIT);
}

void LocaleChangeGuard::CheckLocaleChange(TabContents* tab_contents) {
  // We want notification to be shown no more than once per session.
  if (note_ != NULL)
    return;
  // We check profile Id because:
  // (1) we want to exit fast in common case when nothing should be done.
  // (2) on ChromeOS this guard may be invoked for a dummy profile first time.
  ProfileId cur_profile_id = tab_contents->profile()->GetRuntimeId();
  if (cur_profile_id == profile_id_)
    return;
  profile_id_ = cur_profile_id;
  std::string cur_locale = g_browser_process->GetApplicationLocale();
  if (cur_locale.empty())
    return;
  PrefService* prefs = tab_contents->profile()->GetPrefs();
  if (prefs == NULL)
    return;
  to_locale_ = prefs->GetString(prefs::kApplicationLocaleOverride);
  if (!to_locale_.empty()) {
    DCHECK(to_locale_ == cur_locale);
    return;
  }
  to_locale_ = prefs->GetString(prefs::kApplicationLocale);
  if (to_locale_ != cur_locale)
    return;
  from_locale_ = prefs->GetString(prefs::kApplicationLocaleBackup);
  if (from_locale_.empty() || from_locale_ == to_locale_)
    return;
  note_.reset(new chromeos::SystemNotification(
      tab_contents->profile(),
      new Delegate(this),
      IDR_DEFAULT_FAVICON,
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_SECTION_TITLE_LANGUAGE)));
  tab_contents_ = tab_contents;
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
  // Check whether locale has been reverted or changed.
  // If not: mark current locale as accepted.
  if (tab_contents_ == NULL)
    return;
  if (reverted_)
    return;
  PrefService* prefs = tab_contents_->profile()->GetPrefs();
  if (prefs == NULL)
    return;
  std::string override_locale =
      prefs->GetString(prefs::kApplicationLocaleOverride);
  if (!override_locale.empty())
    return;
  if (prefs->GetString(prefs::kApplicationLocale) != to_locale_)
    return;
  UserMetrics::RecordAction(UserMetricsAction("LanguageChange_Accept"));
  prefs->SetString(prefs::kApplicationLocaleBackup, to_locale_);
  prefs->SetString(prefs::kApplicationLocaleAccepted, to_locale_);
  prefs->ScheduleSavePersistentPrefs();
}

// static
void LocaleChangeGuard::Check(TabContents* tab_contents) {
  g_locale_change_guard.Get().CheckLocaleChange(tab_contents);
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
