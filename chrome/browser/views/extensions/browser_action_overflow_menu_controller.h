// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_EXTENSIONS_BROWSER_ACTION_OVERFLOW_MENU_CONTROLLER_H_
#define CHROME_BROWSER_VIEWS_EXTENSIONS_BROWSER_ACTION_OVERFLOW_MENU_CONTROLLER_H_

#include <vector>

#include "views/controls/menu/menu_delegate.h"

class BrowserActionsContainer;
class BrowserActionView;
class ExtensionActionContextMenu;

class BrowserActionOverflowMenuController : public views::MenuDelegate {
 public:
  BrowserActionOverflowMenuController(
      BrowserActionsContainer* owner,
      views::MenuButton* menu_button,
      const std::vector<BrowserActionView*>& views,
      int start_index);
  virtual ~BrowserActionOverflowMenuController();

  // Shows the overflow menu.
  bool RunMenu(gfx::NativeWindow window);

  // Closes the overflow menu (and its context menu if open as well).
  void CancelMenu();

  // Overridden from views::MenuDelegate:
  virtual void ExecuteCommand(int id);
  virtual bool ShowContextMenu(views::MenuItemView* source,
                               int id,
                               int x,
                               int y,
                               bool is_mouse_gesture);

 private:
  // A pointer to the browser action container that owns the overflow menu.
  BrowserActionsContainer* owner_;

  // A pointer to the overflow menu button that we are showing the menu for.
  views::MenuButton* menu_button_;

  // The overflow menu for the menu button.
  scoped_ptr<views::MenuItemView> menu_;

  // The context menu (when you right click a menu item in the overflow menu).
  scoped_ptr<ExtensionActionContextMenu> context_menu_;

  // The views vector of all the browser actions the container knows about. We
  // won't show all items, just the one starting at |start_index| and above.
  const std::vector<BrowserActionView*>* views_;

  // The index into the BrowserActionView vector, indicating where to start
  // picking browser actions to draw.
  int start_index_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionOverflowMenuController);
};

#endif  // CHROME_BROWSER_VIEWS_EXTENSIONS_BROWSER_ACTION_OVERFLOW_MENU_CONTROLLER_H_
