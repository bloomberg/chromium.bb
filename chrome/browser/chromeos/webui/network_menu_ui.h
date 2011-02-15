// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEBUI_NETWORK_MENU_UI_H_
#define CHROME_BROWSER_CHROMEOS_WEBUI_NETWORK_MENU_UI_H_
#pragma once

#include "chrome/browser/chromeos/webui/menu_ui.h"

namespace ui {
class MenuModel;
} // namespace ui

namespace views {
class Menu2;
}  // namespace views

namespace chromeos {

class NetworkMenuUI : public MenuUI {
 public:
  explicit NetworkMenuUI(TabContents* contents);

  // A callback method that is invoked when the JavaScript wants
  // to invoke an action in the model.
  // By convention the first member of 'values' describes the action.
  // Returns true if the menu should be closed.
  bool ModelAction(const ui::MenuModel* model,
                   const ListValue* values);

  // MenuUI overrides
  virtual DictionaryValue* CreateMenuItem(const ui::MenuModel* model,
                                          int index,
                                          const char* type,
                                          int* max_icon_width,
                                          bool* has_accel) const;

  // A convenient factory method to create Menu2 for the network menu.
  static views::Menu2* CreateMenu2(ui::MenuModel* model);

 private:

  DISALLOW_COPY_AND_ASSIGN(NetworkMenuUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WEBUI_NETWORK_MENU_UI_H_
