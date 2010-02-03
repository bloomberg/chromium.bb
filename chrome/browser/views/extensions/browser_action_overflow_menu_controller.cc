// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/extensions/browser_action_overflow_menu_controller.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/browser_actions_container.h"
#include "chrome/common/extensions/extension.h"
#include "views/controls/menu/menu_item_view.h"

BrowserActionOverflowMenuController::BrowserActionOverflowMenuController(
    BrowserActionsContainer* owner,
    views::MenuButton* menu_button,
    const std::vector<BrowserActionView*>& views,
    int start_index)
    : owner_(owner),
      menu_button_(menu_button),
      views_(&views),
      start_index_(start_index) {
  menu_.reset(new views::MenuItemView(this));
  menu_->set_has_icons(true);

  TabContents* tab = BrowserList::GetLastActive()->GetSelectedTabContents();
  int tab_id = tab->controller().session_id().id();

  size_t command_id = 0;
  for (size_t i = start_index; i < views_->size(); ++i) {
    BrowserActionView* view = (*views_)[i];
    SkBitmap icon =
        view->button()->extension()->browser_action()->GetIcon(tab_id);
    if (icon.isNull())
      icon = view->button()->default_icon();
    menu_->AppendMenuItemWithIcon(
        command_id,
        UTF8ToWide(view->button()->extension()->name()),
        icon);
    ++command_id;
  }
}

BrowserActionOverflowMenuController::~BrowserActionOverflowMenuController() {
}

bool BrowserActionOverflowMenuController::RunMenu(gfx::NativeWindow window) {
  gfx::Rect bounds = menu_button_->GetBounds(
      views::View::IGNORE_MIRRORING_TRANSFORMATION);
  gfx::Point screen_loc;
  views::View::ConvertPointToScreen(menu_button_, &screen_loc);
  bounds.set_x(screen_loc.x());
  bounds.set_y(screen_loc.y());

  views::MenuItemView::AnchorPosition anchor =
      menu_button_->UILayoutIsRightToLeft() ? views::MenuItemView::TOPRIGHT :
                                              views::MenuItemView::TOPLEFT;
  menu_->RunMenuAt(window, menu_button_, bounds, anchor, false);
  return true;
}

void BrowserActionOverflowMenuController::CancelMenu() {
  if (context_menu_.get())
    context_menu_->Cancel();
  menu_->Cancel();
}

void BrowserActionOverflowMenuController::ExecuteCommand(int id) {
  BrowserActionView* view = (*views_)[start_index_ + id];
  owner_->OnBrowserActionExecuted(view->button());
}

bool BrowserActionOverflowMenuController::ShowContextMenu(
    views::MenuItemView* source, int id, int x, int y, bool is_mouse_gesture) {
  if (!context_menu_.get())
    context_menu_.reset(new ExtensionActionContextMenu());
  // This blocks until the user choses something or dismisses.
  context_menu_->Run((*views_)[start_index_ + id]->button()->extension(),
                     gfx::Point(x, y));

  // The user is done with the context menu, so we can close the underlying
  // menu.
  menu_->Cancel();

  return true;
}
