// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LANGUAGE_SWITCH_MODEL_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LANGUAGE_SWITCH_MODEL_H_

#include <string>

#include "app/menus/simple_menu_model.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/language_combobox_model.h"
#include "views/controls/menu/menu_2.h"
#include "views/controls/menu/view_menu_delegate.h"
#include "views/view.h"

namespace chromeos {

class ScreenObserver;

class LanguageSwitchModel : public views::ViewMenuDelegate,
                            public menus::SimpleMenuModel::Delegate {
 public:
  LanguageSwitchModel(ScreenObserver* observer,
                      ScreenObserver::ExitCodes new_state);

  // Initializes language selection menu contents.
  void InitLanguageMenu();

  // Returns current locale name to be placed on the language menu-button.
  std::wstring GetCurrentLocaleName() const;

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

  // Notifications receiver.
  ScreenObserver* observer_;

  // Language locale name storage.
  LanguageList language_list_;

  ScreenObserver::ExitCodes new_state_;

  DISALLOW_COPY_AND_ASSIGN(LanguageSwitchModel);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LANGUAGE_SWITCH_MODEL_H_
