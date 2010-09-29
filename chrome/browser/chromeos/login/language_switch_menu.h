// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LANGUAGE_SWITCH_MENU_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LANGUAGE_SWITCH_MENU_H_
#pragma once

#include <string>

#include "app/menus/simple_menu_model.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/language_combobox_model.h"
#include "testing/gtest/include/gtest/gtest_prod.h"
#include "views/controls/menu/menu_2.h"
#include "views/controls/menu/view_menu_delegate.h"
#include "views/view.h"

class WizardControllerTest_SwitchLanguage_Test;

namespace chromeos {

class ScreenObserver;

class LanguageSwitchMenu : public views::ViewMenuDelegate,
                           public menus::SimpleMenuModel::Delegate {
 public:
  LanguageSwitchMenu();

  // Initializes language selection menu contents.
  void InitLanguageMenu();

  // Returns current locale name to be placed on the language menu-button.
  std::wstring GetCurrentLocaleName() const;

  // Sets the minimum width of the first level menu to be shown.
  void SetFirstLevelMenuWidth(int width);

  void set_menu_offset(int delta_x, int delta_y) {
    delta_x_ = delta_x;
    delta_y_ = delta_y;
  }

  // Switches the current locale, saves the new locale in preferences.
  static void SwitchLanguage(const std::string& locale);

 private:
  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* source, const gfx::Point& pt);

  // menus::SimpleMenuModel::Delegate implementation.
  virtual bool IsCommandIdChecked(int command_id) const;
  virtual bool IsCommandIdEnabled(int command_id) const;
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          menus::Accelerator* accelerator);
  virtual void ExecuteCommand(int command_id);

  // Dialog controls that we own ourselves.
  menus::SimpleMenuModel menu_model_;
  menus::SimpleMenuModel menu_model_submenu_;
  scoped_ptr<views::Menu2> menu_;

  // Language locale name storage.
  scoped_ptr<LanguageList> language_list_;

  int delta_x_, delta_y_;

  FRIEND_TEST(::WizardControllerTest, SwitchLanguage);
  DISALLOW_COPY_AND_ASSIGN(LanguageSwitchMenu);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LANGUAGE_SWITCH_MENU_H_
