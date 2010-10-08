// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_VIEWS_NATIVE_MENU_DOMUI_H_
#define CHROME_BROWSER_CHROMEOS_VIEWS_NATIVE_MENU_DOMUI_H_
#pragma once

#include <vector>

#include "base/message_loop.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/dom_ui/domui_menu_control.h"
#include "views/controls/menu/menu_wrapper.h"

class SkBitmap;
class DictionaryValue;
class Profile;

namespace menus {
class MenuModel;
}  // namespace menus

namespace chromeos {

class MenuLocator;
class DOMUIMenuWidget;

// A DOMUI implementation of MenuWrapper.
class NativeMenuDOMUI : public views::MenuWrapper,
                        public DOMUIMenuControl,
                        public MessageLoopForUI::Dispatcher {
 public:
  NativeMenuDOMUI(menus::MenuModel* menu_model, bool root);
  virtual ~NativeMenuDOMUI();

  // Returns true if menu is currently shown.
  bool is_menu_shown() { return menu_shown_; }

  // Set parent menu.
  void set_parent(NativeMenuDOMUI* parent) { parent_ = parent; }

  // Overridden from MenuWrapper:
  virtual void RunMenuAt(const gfx::Point& point, int alignment);
  virtual void CancelMenu();
  virtual void Rebuild();
  virtual void UpdateStates();
  virtual gfx::NativeMenu GetNativeMenu() const;
  virtual MenuAction GetMenuAction() const;
  virtual void AddMenuListener(views::MenuListener* listener);
  virtual void RemoveMenuListener(views::MenuListener* listener);
  virtual void SetMinimumWidth(int width);

  // Overriden from MessageLoopForUI::Dispatcher:
  virtual bool Dispatch(GdkEvent* event);

  // Overriden from DOMUIMenuControl;
  virtual menus::MenuModel* GetMenuModel() { return model_; }
  virtual void Activate(menus::MenuModel* model,
                        int index,
                        ActivationMode activation_mode);
  virtual void CloseAll();
  virtual void CloseSubmenu();
  virtual void MoveInputToParent();
  virtual void MoveInputToSubmenu();
  virtual void OnLoad();
  virtual void OpenSubmenu(int index, int y);
  virtual void SetSize(const gfx::Size& size);

  // Hide All menu (including submenus).
  void Hide();

  // Returns the root of the menu tree. Returns NULL if it cannot find
  // a root. (i.e. detached from root)
  NativeMenuDOMUI* GetRoot();

  // Returns the profile to create DOMView.
  Profile* GetProfile();

 private:
  // Callback that we should really process the menu activation.
  // See description above class for why we delay processing activation.
  void ProcessActivate();

  // Creates a menu item for the menu item at index.
  DictionaryValue* CreateMenuItem(int index,
                                  const char* type,
                                  bool* has_icon_out);

  // Show the menu using given |locator|.
  void ShowAt(MenuLocator* locator);

  // Find a menu object at point.
  NativeMenuDOMUI* FindMenuAt(const gfx::Point& point);

  // If we're a submenu, this is the parent.
  NativeMenuDOMUI* parent_;

  // Holds the current submenu.
  scoped_ptr<NativeMenuDOMUI> submenu_;

  menus::MenuModel* model_;

  // A window widget that draws the content of the menu.
  DOMUIMenuWidget* menu_widget_;

  // True if the menu is currently shown.
  // Used only in root.
  bool menu_shown_;

  // If the user selects something from the menu this is the menu they selected
  // it from. When an item is selected menu_activated_ on the root ancestor is
  // set to the menu the user selected and after the nested message loop exits
  // Activate is invoked on this menu.
  menus::MenuModel* activated_menu_;

  // The index of the item the user selected. This is set on the
  // actual menu the user selects and not the root.
  int activated_index_;

  // The action that took place during the call to RunMenuAt.
  MenuAction menu_action_;

  // Vector of listeners to receive callbacks when the menu opens.
  ObserverList<views::MenuListener> listeners_;

  DISALLOW_COPY_AND_ASSIGN(NativeMenuDOMUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_VIEWS_NATIVE_MENU_DOMUI_H_
