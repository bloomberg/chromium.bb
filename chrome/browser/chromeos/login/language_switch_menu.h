// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LANGUAGE_SWITCH_MENU_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LANGUAGE_SWITCH_MENU_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/language_combobox_model.h"
#include "testing/gtest/include/gtest/gtest_prod.h"
#include "ui/base/models/simple_menu_model.h"
#include "views/controls/menu/menu_2.h"
#include "views/controls/menu/view_menu_delegate.h"
#include "views/view.h"

class WizardControllerTest_SwitchLanguage_Test;

namespace chromeos {

class ScreenObserver;

class LanguageSwitchMenu : public views::ViewMenuDelegate,
                           public ui::SimpleMenuModel::Delegate {
 public:
  LanguageSwitchMenu();

  // Initializes language selection menu contents.
  void InitLanguageMenu();

  // Sets menu's alignment.
  void set_menu_alignment(views::Menu2::Alignment alignment) {
    menu_alignment_ = alignment;
  }

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
  virtual void RunMenu(views::View* source, const gfx::Point& pt);

  // ui::SimpleMenuModel::Delegate implementation.
  virtual bool IsCommandIdChecked(int command_id) const;
  virtual bool IsCommandIdEnabled(int command_id) const;
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          ui::Accelerator* accelerator);
  virtual void ExecuteCommand(int command_id);

  // Dialog controls that we own ourselves.
  ui::SimpleMenuModel menu_model_;
  ui::SimpleMenuModel menu_model_submenu_;
  scoped_ptr<views::Menu2> menu_;

  // Language locale name storage.
  scoped_ptr<LanguageList> language_list_;

  // Menu alignment.
  views::Menu2::Alignment menu_alignment_;

  FRIEND_TEST(::WizardControllerTest, SwitchLanguage);
  DISALLOW_COPY_AND_ASSIGN(LanguageSwitchMenu);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LANGUAGE_SWITCH_MENU_H_
