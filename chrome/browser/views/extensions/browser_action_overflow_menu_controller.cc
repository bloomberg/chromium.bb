// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/extensions/browser_action_overflow_menu_controller.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/browser_actions_container.h"
#include "chrome/browser/views/extensions/browser_action_drag_data.h"
#include "chrome/common/extensions/extension.h"
#include "views/controls/menu/menu_item_view.h"

BrowserActionOverflowMenuController::BrowserActionOverflowMenuController(
    BrowserActionsContainer* owner,
    views::MenuButton* menu_button,
    const std::vector<BrowserActionView*>& views,
    int start_index)
    : owner_(owner),
      observer_(NULL),
      menu_button_(menu_button),
      views_(&views),
      start_index_(start_index),
      for_drop_(false) {
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
  if (observer_)
    observer_->NotifyMenuDeleted(this);
}

bool BrowserActionOverflowMenuController::RunMenu(gfx::NativeWindow window,
                                                  bool for_drop) {
  for_drop_ = for_drop;

  gfx::Rect bounds = menu_button_->GetBounds(
      views::View::IGNORE_MIRRORING_TRANSFORMATION);
  gfx::Point screen_loc;
  views::View::ConvertPointToScreen(menu_button_, &screen_loc);
  bounds.set_x(screen_loc.x());
  bounds.set_y(screen_loc.y());

  views::MenuItemView::AnchorPosition anchor =
      menu_button_->UILayoutIsRightToLeft() ? views::MenuItemView::TOPRIGHT :
                                              views::MenuItemView::TOPLEFT;
  if (for_drop) {
    menu_->RunMenuForDropAt(window, bounds, anchor);
  } else {
    menu_->RunMenuAt(window, menu_button_, bounds, anchor, false);
    delete this;
  }
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

void BrowserActionOverflowMenuController::DropMenuClosed(
    views::MenuItemView* menu) {
  delete this;
}

bool BrowserActionOverflowMenuController::GetDropFormats(
    views::MenuItemView* menu,
    int* formats,
    std::set<OSExchangeData::CustomFormat>* custom_formats) {
  custom_formats->insert(BrowserActionDragData::GetBrowserActionCustomFormat());
  return true;
}

bool BrowserActionOverflowMenuController::AreDropTypesRequired(
    views::MenuItemView* menu) {
  return true;
}

bool BrowserActionOverflowMenuController::CanDrop(
    views::MenuItemView* menu, const OSExchangeData& data) {
  BrowserActionDragData drop_data;
  if (!drop_data.Read(data))
    return false;
  return drop_data.IsFromProfile(owner_->profile());
}

int BrowserActionOverflowMenuController::GetDropOperation(
    views::MenuItemView* item,
    const views::DropTargetEvent& event,
    DropPosition* position) {
  // Don't allow dropping from the BrowserActionContainer into slot 0 of the
  // overflow menu since once the move has taken place the item you are dragging
  // falls right out of the menu again once the user releases the button
  // (because we don't shrink the BrowserActionContainer when you do this).
  if ((item->GetCommand() == 0) && (*position == DROP_BEFORE)) {
    BrowserActionDragData drop_data;
    if (!drop_data.Read(event.GetData()))
      return DragDropTypes::DRAG_NONE;

    if (drop_data.index() < owner_->VisibleBrowserActions())
      return DragDropTypes::DRAG_NONE;
  }

  return DragDropTypes::DRAG_MOVE;
}

int BrowserActionOverflowMenuController::OnPerformDrop(
    views::MenuItemView* menu,
    DropPosition position,
    const views::DropTargetEvent& event) {
  BrowserActionDragData drop_data;
  if (!drop_data.Read(event.GetData()))
    return DragDropTypes::DRAG_NONE;

  size_t drop_index;
  ViewForId(menu->GetCommand(), &drop_index);

  // When not dragging within the overflow menu (dragging an icon into the menu)
  // subtract one to get the right index.
  if (position == DROP_BEFORE &&
      drop_data.index() < owner_->VisibleBrowserActions())
    --drop_index;

  owner_->MoveBrowserAction(drop_data.id(), drop_index);

  if (for_drop_)
    delete this;
  return DragDropTypes::DRAG_MOVE;
}

bool BrowserActionOverflowMenuController::CanDrag(views::MenuItemView* menu) {
  return true;
}

void BrowserActionOverflowMenuController::WriteDragData(
    views::MenuItemView* sender, OSExchangeData* data) {
  size_t drag_index;
  BrowserActionView* view = ViewForId(sender->GetCommand(), &drag_index);
  std::string id = view->button()->extension()->id();

  BrowserActionDragData drag_data(id, drag_index);
  drag_data.Write(owner_->profile(), data);
}

int BrowserActionOverflowMenuController::GetDragOperations(
    views::MenuItemView* sender) {
  return DragDropTypes::DRAG_MOVE;
}

BrowserActionView* BrowserActionOverflowMenuController::ViewForId(
    int id, size_t* index) {
  // The index of the view being dragged (GetCommand gives a 0-based index into
  // the overflow menu).
  size_t view_index = owner_->VisibleBrowserActions() + id;
  if (index)
    *index = view_index;
  return owner_->GetBrowserActionViewAt(view_index);
}
