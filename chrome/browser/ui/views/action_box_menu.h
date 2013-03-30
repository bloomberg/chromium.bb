// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACTION_BOX_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_ACTION_BOX_MENU_H_

#include "base/memory/scoped_ptr.h"
#include "ui/views/controls/menu/menu_delegate.h"

class ActionBoxContextMenu;
class ActionBoxMenuModel;
class Browser;

namespace views {
class MenuButton;
class MenuItemView;
class MenuRunner;
}

// ActionBoxMenu adapts the ActionBoxMenuModel to view's menu related classes.
class ActionBoxMenu : public views::MenuDelegate {
 public:
  // Constructs and initializes an ActionBoxMenu.
  static scoped_ptr<ActionBoxMenu> Create(Browser* browser,
                                          scoped_ptr<ActionBoxMenuModel> model);

  virtual ~ActionBoxMenu();

  // Shows the menu relative to the specified button.
  void RunMenu(views::MenuButton* menu_button, gfx::Point menu_offset);

 private:
  ActionBoxMenu(Browser* browser, scoped_ptr<ActionBoxMenuModel> model);

  // Overridden from views::MenuDelegate:
  virtual void ExecuteCommand(int id) OVERRIDE;
  virtual bool ShowContextMenu(views::MenuItemView* source,
                               int id,
                               const gfx::Point& p,
                               bool is_mouse_gesture) OVERRIDE;

  // Populates |root_| with all the child menu items from the |model_|.
  void PopulateMenu();

  Browser* browser_;

  scoped_ptr<ActionBoxContextMenu> context_menu_;
  scoped_ptr<views::MenuRunner> menu_runner_;
  views::Widget* menu_parent_;

  // The model that tracks the order of the toolbar icons.
  scoped_ptr<ActionBoxMenuModel> model_;

  DISALLOW_COPY_AND_ASSIGN(ActionBoxMenu);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ACTION_BOX_MENU_H_
