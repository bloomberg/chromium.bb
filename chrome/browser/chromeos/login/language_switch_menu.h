// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LANGUAGE_SWITCH_MENU_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LANGUAGE_SWITCH_MENU_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest_prod.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/view_menu_delegate.h"
#include "ui/views/view.h"

class WizardControllerTest_SwitchLanguage_Test;

namespace views {
class MenuItemView;
class MenuRunner;
}  // namespace views

namespace chromeos {

class LanguageList;

class LanguageSwitchMenu : public views::ViewMenuDelegate,
                           public views::MenuDelegate {
 public:
  LanguageSwitchMenu();
  virtual ~LanguageSwitchMenu();

  // Initializes language selection menu contents.
  void InitLanguageMenu();

  // Returns current locale name to be placed on the language menu-button.
  string16 GetCurrentLocaleName() const;

  // Sets the minimum width of the first level menu to be shown.
  void SetFirstLevelMenuWidth(int width);

  // Switches the current locale, saves the new locale in preferences.
  // Returns true if it has switched the current locale.
  static bool SwitchLanguage(const std::string& locale);

  // Switches the current locale, saves the new locale in preferences.
  // Enables the keyboard layouts associated with the new locale.
  static void SwitchLanguageAndEnableKeyboardLayouts(
      const std::string& locale);

 private:
  static void LoadFontsForCurrentLocale();

  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* source, const gfx::Point& pt) OVERRIDE;

  // views::MenuDelegate implementation.
  virtual void ExecuteCommand(int command_id) OVERRIDE;

  // The menu.
  views::MenuItemView* menu_;

  // Runs and owns |menu_|.
  scoped_ptr<views::MenuRunner> menu_runner_;

  // Language locale name storage.
  scoped_ptr<LanguageList> language_list_;

  FRIEND_TEST(::WizardControllerTest, SwitchLanguage);
  DISALLOW_COPY_AND_ASSIGN(LanguageSwitchMenu);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LANGUAGE_SWITCH_MENU_H_
