// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACTION_BOX_CONTEXT_MENU_H__
#define CHROME_BROWSER_UI_VIEWS_ACTION_BOX_CONTEXT_MENU_H__

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/toolbar/action_box_context_menu_controller.h"
#include "ui/views/controls/menu/menu_runner.h"

class Browser;

namespace extensions {
class Extension;
}  // namespace extensions

namespace gfx {
class Point;
}  // namespace gfx

namespace views {
class MenuModelAdapter;
class Widget;
}  // namespace views

// This is the Views class responsible for showing the context menu for
// extensions in the action box. Actually building and executing commands is
// handled by ActionBoxContextMenuController.
class ActionBoxContextMenu {
 public:
  ActionBoxContextMenu(Browser* browser,
                       const extensions::Extension* extension);
  ~ActionBoxContextMenu();

  // See comments in menu_runner.h on how the return value should be used.
  views::MenuRunner::RunResult RunMenuAt(
      const gfx::Point& p,
      views::Widget* parent_widget) WARN_UNUSED_RESULT;

 private:
  ActionBoxContextMenuController controller_;
  scoped_ptr<views::MenuModelAdapter> adapter_;
  scoped_ptr<views::MenuRunner> menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(ActionBoxContextMenu);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ACTION_BOX_CONTEXT_MENU_H__
