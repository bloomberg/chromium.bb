// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DOM_UI_NETWORK_MENU_UI_H_
#define CHROME_BROWSER_CHROMEOS_DOM_UI_NETWORK_MENU_UI_H_
#pragma once

#include "chrome/browser/chromeos/dom_ui/menu_ui.h"

namespace views {
class Menu2;
}  // namespace views

namespace menus {
class MenuModel;
} // namespace menus

namespace chromeos {

class NetworkMenuUI : public MenuUI {
 public:
  explicit NetworkMenuUI(TabContents* contents);

  // A callback method that is invoked when the JavaScript wants
  // to invoke an action in the model.
  // By convention the first member of 'values' describes the action.
  // Returns true if the menu should be closed.
  bool ModelAction(const menus::MenuModel* model,
                   const ListValue* values);

  // MenuUI overrides
  virtual DictionaryValue* CreateMenuItem(const menus::MenuModel* model,
                                          int index,
                                          const char* type,
                                          int* max_icon_width,
                                          bool* has_accel) const;
  virtual void AddCustomConfigValues(DictionaryValue* config) const;
  virtual void AddLocalizedStrings(DictionaryValue* localized_strings) const;

  // A convenient factory method to create Menu2 for the network menu.
  static views::Menu2* CreateMenu2(menus::MenuModel* model);

 private:

  DISALLOW_COPY_AND_ASSIGN(NetworkMenuUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DOM_UI_NETWORK_MENU_UI_H_
