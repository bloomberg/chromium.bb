// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEBUI_MENU_CONTROL_H_
#define CHROME_BROWSER_CHROMEOS_WEBUI_MENU_CONTROL_H_
#pragma once

namespace gfx {
class Size;
}  // namespace gfx

namespace ui {
class MenuModel;
}  // namespace ui

namespace chromeos {

// WebUIMenuControl class is used to control the UI counterpart of
// a MenuModel. One instance of WebUIMenuControl is created for each
// MenuModel instance, that is, a submenu will have its own
// WebUIMenuControl.
class WebUIMenuControl {
 public:
  enum ActivationMode {
    ACTIVATE_NO_CLOSE,   // Activate the command without closing menu.
    CLOSE_AND_ACTIVATE,  // Close the menu and then activate the command.
  };
  virtual ~WebUIMenuControl() {}

  // Returns the MenuModel associated with this control.
  virtual ui::MenuModel* GetMenuModel() = 0;

  // Activates an item in the |model| at |index|.
  virtual void Activate(ui::MenuModel* model,
                        int index,
                        ActivationMode activation_mode) = 0;

  // Close All menu window from root menu to leaf submenus.
  virtual void CloseAll() = 0;

  // Close the submenu (and all decendant submenus).
  virtual void CloseSubmenu() = 0;

  // Move the input to parent. Used in keyboard navigation.
  virtual void MoveInputToParent() = 0;

  // Move the input to submenu. Used in keyboard navigation.
  virtual void MoveInputToSubmenu() = 0;

  // Called when the menu page is loaded. This is used to call
  // initialize function in JavaScript.
  virtual void OnLoad() = 0;

  // Open submenu using the submenu model at index in the model.
  // The top coordinate of the selected menu is passed as |y_top|
  // so that the submenu can be aligned to the selected item.
  virtual void OpenSubmenu(int index, int y_top) =0;

  // Sets the size of the menu.
  virtual void SetSize(const gfx::Size& size) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WEBUI_MENU_CONTROL_H_
