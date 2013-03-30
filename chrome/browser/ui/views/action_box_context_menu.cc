// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/action_box_context_menu.h"

#include "chrome/browser/ui/toolbar/action_box_context_menu_controller.h"
#include "ui/gfx/point.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"

using views::MenuRunner;

ActionBoxContextMenu::ActionBoxContextMenu(
    Browser* browser,
    const extensions::Extension* extension)
    : controller_(browser, extension) {
}

ActionBoxContextMenu::~ActionBoxContextMenu() {
}

views::MenuRunner::RunResult ActionBoxContextMenu::RunMenuAt(
    const gfx::Point& p,
    views::Widget* parent_widget) {
  adapter_.reset(new views::MenuModelAdapter(controller_.menu_model()));
  menu_runner_.reset(new MenuRunner(adapter_->CreateMenu()));
  return menu_runner_->RunMenuAt(
      parent_widget,
      NULL,  // No menu button.
      gfx::Rect(p, gfx::Size()),
      views::MenuItemView::TOPLEFT,
      (MenuRunner::CONTEXT_MENU | MenuRunner::IS_NESTED |
       MenuRunner::HAS_MNEMONICS));
}
