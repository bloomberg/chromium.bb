// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/language_switch_menu.h"

#include "base/i18n/rtl.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/login/language_list.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "grit/platform_locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/platform_font_pango.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/menu/menu_item_view.h"
#include "views/controls/menu/menu_runner.h"
#include "views/controls/menu/submenu_view.h"
#include "views/widget/widget.h"

namespace {

const int kLanguageMainMenuSize = 5;
// TODO(glotov): need to specify the list as a part of the image customization.
const char kLanguagesTopped[] = "es,it,de,fr,en-US";
const int kMoreLanguagesSubMenu = 200;

}  // namespace

namespace chromeos {

LanguageSwitchMenu::LanguageSwitchMenu()
    : ALLOW_THIS_IN_INITIALIZER_LIST(menu_(new views::MenuItemView(this))),
      menu_runner_(new views::MenuRunner(menu_)) {
}

LanguageSwitchMenu::~LanguageSwitchMenu() {}

void LanguageSwitchMenu::InitLanguageMenu() {
  // Update LanguageList to contain entries in current locale.
  language_list_.reset(new LanguageList);
  language_list_->CopySpecifiedLanguagesUp(kLanguagesTopped);

  // Clear older menu items.
  if (menu_->HasSubmenu()) {
    const int old_count = menu_->GetSubmenu()->child_count();
    for (int i = 0; i < old_count; ++i)
      menu_->RemoveMenuItemAt(0);
  }

  // Fill menu items with updated items.
  for (int line = 0; line != kLanguageMainMenuSize; line++) {
    menu_->AppendMenuItemWithLabel(line,
                                   language_list_->GetLanguageNameAt(line));
  }

  menu_->AppendSeparator();
  views::MenuItemView* submenu = menu_->AppendSubMenu(
      kMoreLanguagesSubMenu,
      l10n_util::GetStringUTF16(IDS_LANGUAGES_MORE));

  for (int line = kLanguageMainMenuSize;
       line != language_list_->languages_count(); ++line) {
    submenu->AppendMenuItemWithLabel(line,
                                     language_list_->GetLanguageNameAt(line));
  }

  menu_->ChildrenChanged();
}

string16 LanguageSwitchMenu::GetCurrentLocaleName() const {
  DCHECK(g_browser_process);
  const std::string locale = g_browser_process->GetApplicationLocale();
  int index = language_list_->GetIndexFromLocale(locale);
  CHECK_NE(-1, index) << "Unknown locale: " << locale;
  return language_list_->GetLanguageNameAt(index);
};

void LanguageSwitchMenu::SetFirstLevelMenuWidth(int width) {
  DCHECK(menu_ != NULL);

  menu_->GetSubmenu()->set_minimum_preferred_width(width);
}

// static
bool LanguageSwitchMenu::SwitchLanguage(const std::string& locale) {
  DCHECK(g_browser_process);
  if (g_browser_process->GetApplicationLocale() == locale) {
    return false;
  }
  // TODO(markusheintz): Change the if condition to prefs->IsUserModifiable()
  // once Mattias landed his pending patch.
  if (!g_browser_process->local_state()->
      IsManagedPreference(prefs::kApplicationLocale)) {
    std::string loaded_locale;
    {
      // Reloading resource bundle causes us to do blocking IO on UI thread.
      // Temporarily allow it until we fix http://crosbug.com/11102
      base::ThreadRestrictions::ScopedAllowIO allow_io;
      // Switch the locale.
      loaded_locale = ResourceBundle::ReloadSharedInstance(locale);
    }
    CHECK(!loaded_locale.empty()) << "Locale could not be found for " << locale;

    LoadFontsForCurrentLocale();

    // The following line does not seem to affect locale anyhow. Maybe in
    // future..
    g_browser_process->SetApplicationLocale(locale);

    // Force preferences save, otherwise they won't be saved on
    // shutdown from login screen. http://crosbug.com/20747
    PrefService* prefs = g_browser_process->local_state();
    prefs->SetString(prefs::kApplicationLocale, locale);
    prefs->ScheduleSavePersistentPrefs();

    return true;
  }
  return false;
}

// static
void LanguageSwitchMenu::LoadFontsForCurrentLocale() {
#if defined(TOOLKIT_USES_GTK)
  std::string gtkrc = l10n_util::GetStringUTF8(IDS_LOCALE_GTKRC);

  // Read locale-specific gtkrc.  Ideally we'd discard all the previously read
  // gtkrc information, but GTK doesn't support that.  Reading the new locale's
  // gtkrc overrides the styles from previous ones when there is a conflict, but
  // styles that are added and not conflicted will not be overridden.  So far
  // there are no locales with such a thing; if there are then this solution
  // will not work.
  if (!gtkrc.empty())
    gtk_rc_parse_string(gtkrc.c_str());
  else
    gtk_rc_parse("/etc/gtk-2.0/gtkrc");
#else
  // TODO(saintlou): Need to figure out an Aura equivalent.
  NOTIMPLEMENTED();
#endif

  // Switch the font.
  gfx::PlatformFontPango::ReloadDefaultFont();
  ResourceBundle::GetSharedInstance().ReloadFonts();
}

// static
void LanguageSwitchMenu::SwitchLanguageAndEnableKeyboardLayouts(
    const std::string& locale) {
  if (SwitchLanguage(locale)) {
    // If we have switched the locale, enable the keyboard layouts that
    // are necessary for the new locale.  Change the current input method
    // to the hardware keyboard layout since the input method currently in
    // use may not be supported by the new locale (3rd parameter).
    input_method::InputMethodManager* manager =
        input_method::InputMethodManager::GetInstance();
    manager->EnableInputMethods(
        locale, input_method::kKeyboardLayoutsOnly,
        manager->GetInputMethodUtil()->GetHardwareInputMethodId());
  }
}

////////////////////////////////////////////////////////////////////////////////
// views::ViewMenuDelegate implementation.

void LanguageSwitchMenu::RunMenu(views::View* source, const gfx::Point& pt) {
  DCHECK(menu_ != NULL);
  views::MenuButton* button = static_cast<views::MenuButton*>(source);

  // We align on the left edge of the button for non RTL case.
  // MenuButton passes in pt the lower left corner for RTL and the
  // lower right corner for non-RTL (with menu_offset applied).
  const int reverse_offset = button->width() + button->menu_offset().x() * 2;
  gfx::Point new_pt(pt);
  if (base::i18n::IsRTL())
    new_pt.set_x(pt.x() + reverse_offset);
  else
    new_pt.set_x(pt.x() - reverse_offset);

  if (menu_runner_->RunMenuAt(button->GetWidget(), button,
          gfx::Rect(new_pt, gfx::Size()), views::MenuItemView::TOPLEFT,
          views::MenuRunner::HAS_MNEMONICS) == views::MenuRunner::MENU_DELETED)
    return;
}

////////////////////////////////////////////////////////////////////////////////
// views::MenuDelegate implementation.

void LanguageSwitchMenu::ExecuteCommand(int command_id) {
  const std::string locale = language_list_->GetLocaleFromIndex(command_id);
  // Here, we should enable keyboard layouts associated with the locale so
  // that users can use those keyboard layouts on the login screen.
  SwitchLanguageAndEnableKeyboardLayouts(locale);
  InitLanguageMenu();

  // Update all view hierarchies that the locale has changed.
  views::Widget::NotifyLocaleChanged();
}

}  // namespace chromeos
