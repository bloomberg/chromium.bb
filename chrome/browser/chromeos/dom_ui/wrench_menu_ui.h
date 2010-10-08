// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DOM_UI_WRENCH_MENU_UI_H_
#define CHROME_BROWSER_CHROMEOS_DOM_UI_WRENCH_MENU_UI_H_
#pragma once

#include "chrome/browser/chromeos/dom_ui/menu_ui.h"

namespace views {
class Menu2;
}  // namespace views

namespace menus {
class MenuModel;
} // namespace menus

namespace chromeos {

class WrenchMenuUI : public MenuUI {
 public:
  explicit WrenchMenuUI(TabContents* contents);

  // MenuUI overrides:
  virtual void AddCustomConfigValues(DictionaryValue* config) const;

  // A convenient factory method to create Menu2 for wrench menu.
  static views::Menu2* CreateMenu2(menus::MenuModel* model);

 private:
  DISALLOW_COPY_AND_ASSIGN(WrenchMenuUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DOM_UI_WRENCH_MENU_UI_H_
