// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/language_switch_model.h"

#include "app/l10n_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/pref_service.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"

namespace {
const int kLanguageMainMenuSize = 5;
}  // namespace

namespace chromeos {

LanguageSwitchModel::LanguageSwitchModel(ScreenObserver* observer)
    : ALLOW_THIS_IN_INITIALIZER_LIST(menu_model_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(menu_model_submenu_(this)),
      menu_(NULL),
      observer_(observer) {
  // TODO(glotov): need to specify the following list as a part of the
  // image customization.
  language_list_.CopySpecifiedLanguagesUp("es,it,de,fr,en-US");
}

void LanguageSwitchModel::InitLanguageMenu() {
  for (int line = 0; line != kLanguageMainMenuSize; line++) {
    menu_model_.AddItem(
        line, WideToUTF16(language_list_.GetLanguageNameAt(line)));
  }
  menu_model_.AddSeparator();
  menu_model_.AddSubMenu(WideToUTF16(l10n_util::GetString(IDS_LANGUAGES_MORE)),
                         &menu_model_submenu_);
  for (int line = kLanguageMainMenuSize;
       line != language_list_.get_languages_count(); line++) {
    menu_model_submenu_.AddItem(
        line, WideToUTF16(language_list_.GetLanguageNameAt(line)));
  }

  // Initialize menu here so it appears fast when called.
  menu_.reset(new views::Menu2(&menu_model_));
}

std::wstring LanguageSwitchModel::GetCurrentLocaleName() const {
  DCHECK(g_browser_process);
  const std::string locale = g_browser_process->GetApplicationLocale();
  return language_list_.GetLanguageNameAt(
      language_list_.GetIndexFromLocale(locale));
};

////////////////////////////////////////////////////////////////////////////////
// views::ViewMenuDelegate implementation.

void LanguageSwitchModel::RunMenu(views::View* source, const gfx::Point& pt) {
  DCHECK(menu_ != NULL);
  menu_->RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
}

////////////////////////////////////////////////////////////////////////////////
// menus::SimpleMenuModel::Delegate implementation.

bool LanguageSwitchModel::IsCommandIdChecked(int command_id) const {
  return false;
}

bool LanguageSwitchModel::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool LanguageSwitchModel::GetAcceleratorForCommandId(
    int command_id, menus::Accelerator* accelerator) {
  return false;
}

void LanguageSwitchModel::ExecuteCommand(int command_id) {
  const std::string locale = language_list_.GetLocaleFromIndex(command_id);
  DCHECK(g_browser_process);
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetString(prefs::kApplicationLocale, UTF8ToWide(locale));
  prefs->SavePersistentPrefs();
  observer_->OnSwitchLanguage(locale);
  // Don't do anything here because |this| has just been deleted in order
  // to force releasing all locale-specific data.
}

}  // namespace chromeos
