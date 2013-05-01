// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/language_switch_menu.h"

#include "ash/shell.h"
#include "base/i18n/rtl.h"
#include "base/prefs/pref_service.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/login/language_list.h"
#include "chrome/browser/chromeos/login/screens/screen_observer.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ime/input_method_manager.h"
#include "grit/generated_resources.h"
#include "grit/platform_locale_settings.h"
#include "ui/aura/root_window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/platform_font_pango.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/widget/widget.h"

namespace {

const int kLanguageMainMenuSize = 5;
// TODO(glotov): need to specify the list as a part of the image customization.
const char kLanguagesTopped[] = "es,it,de,fr,en-US";
const int kMoreLanguagesSubMenu = 200;

// Notifies all views::Widgets attached to |window|'s hierarchy that their
// locale has changed.
void NotifyLocaleChanged(aura::Window* window) {
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  if (widget)
    widget->LocaleChanged();

  const aura::Window::Windows& children = window->children();
  for (aura::Window::Windows::const_iterator it = children.begin();
       it != children.end(); ++it) {
    NotifyLocaleChanged(*it);
  }
}

}  // namespace

namespace chromeos {

LanguageSwitchMenu::LanguageSwitchMenu()
    : menu_(new views::MenuItemView(this)),
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
      loaded_locale =
          ResourceBundle::GetSharedInstance().ReloadLocaleResources(locale);
    }
    CHECK(!loaded_locale.empty()) << "Locale could not be found for " << locale;

    LoadFontsForCurrentLocale();

    // The following line does not seem to affect locale anyhow. Maybe in
    // future..
    g_browser_process->SetApplicationLocale(locale);

    PrefService* prefs = g_browser_process->local_state();
    prefs->SetString(prefs::kApplicationLocale, locale);

    return true;
  }
  return false;
}

// static
void LanguageSwitchMenu::LoadFontsForCurrentLocale() {
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
        input_method::GetInputMethodManager();
    manager->EnableLayouts(
        locale,
        manager->GetInputMethodUtil()->GetHardwareInputMethodId());
  }
}

////////////////////////////////////////////////////////////////////////////////
// views::MenuButtonListener implementation.

void LanguageSwitchMenu::OnMenuButtonClicked(views::View* source,
                                             const gfx::Point& point) {
  DCHECK(menu_ != NULL);
  views::MenuButton* button = static_cast<views::MenuButton*>(source);

  // We align on the left edge of the button for non RTL case.
  // MenuButton passes in |point| the lower left corner for RTL and the
  // lower right corner for non-RTL (with menu_offset applied).
  const int reverse_offset = button->width() + button->menu_offset().x() * 2;
  gfx::Point new_pt(point);
  if (base::i18n::IsRTL())
    new_pt.set_x(point.x() + reverse_offset);
  else
    new_pt.set_x(point.x() - reverse_offset);

  if (menu_runner_->RunMenuAt(button->GetWidget(), button,
          gfx::Rect(new_pt, gfx::Size()), views::MenuItemView::TOPLEFT,
          views::MenuRunner::HAS_MNEMONICS) == views::MenuRunner::MENU_DELETED)
    return;
}

////////////////////////////////////////////////////////////////////////////////
// views::MenuDelegate implementation.

void LanguageSwitchMenu::ExecuteCommand(int command_id, int event_flags) {
  const std::string locale = language_list_->GetLocaleFromIndex(command_id);
  // Here, we should enable keyboard layouts associated with the locale so
  // that users can use those keyboard layouts on the login screen.
  SwitchLanguageAndEnableKeyboardLayouts(locale);
  InitLanguageMenu();
  NotifyLocaleChanged(ash::Shell::GetPrimaryRootWindow());
}

}  // namespace chromeos
