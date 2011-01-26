// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/browser_action_overflow_menu_controller.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/browser_actions_container.h"
#include "chrome/browser/ui/views/extensions/browser_action_drag_data.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "gfx/canvas_skia.h"
#include "views/controls/menu/menu_item_view.h"
#include "views/controls/menu/menu_2.h"

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

  size_t command_id = 1;  // Menu id 0 is reserved, start with 1.
  for (size_t i = start_index; i < views_->size(); ++i) {
    BrowserActionView* view = (*views_)[i];
    scoped_ptr<gfx::Canvas> canvas(view->GetIconWithBadge());
    menu_->AppendMenuItemWithIcon(
        command_id,
        UTF8ToWide(view->button()->extension()->name()),
        canvas->AsCanvasSkia()->ExtractBitmap());

    // Set the tooltip for this item.
    std::wstring tooltip = UTF8ToWide(
        view->button()->extension()->browser_action()->GetTitle(
            owner_->GetCurrentTabId()));
    menu_->SetTooltip(tooltip, command_id);

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

  views::MenuItemView::AnchorPosition anchor = base::i18n::IsRTL() ?
      views::MenuItemView::TOPLEFT : views::MenuItemView::TOPRIGHT;
  if (for_drop) {
    menu_->RunMenuForDropAt(window, bounds, anchor);
  } else {
    menu_->RunMenuAt(window, menu_button_, bounds, anchor, false);
    // Give the context menu (if any) a chance to execute the user-selected
    // command.
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }
  return true;
}

void BrowserActionOverflowMenuController::CancelMenu() {
  menu_->Cancel();
}

void BrowserActionOverflowMenuController::ExecuteCommand(int id) {
  BrowserActionView* view = (*views_)[start_index_ + id - 1];
  owner_->OnBrowserActionExecuted(view->button(),
                                  false);  // inspect_with_devtools
}

bool BrowserActionOverflowMenuController::ShowContextMenu(
    views::MenuItemView* source,
    int id,
    const gfx::Point& p,
    bool is_mouse_gesture) {
  const Extension* extension =
      (*views_)[start_index_ + id - 1]->button()->extension();
  if (!extension->ShowConfigureContextMenus())
    return false;

  context_menu_contents_ = new ExtensionContextMenuModel(
      extension,
      owner_->browser(),
      owner_);
  context_menu_menu_.reset(new views::Menu2(context_menu_contents_.get()));
  // This blocks until the user choses something or dismisses the menu.
  context_menu_menu_->RunContextMenuAt(p);

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
      return ui::DragDropTypes::DRAG_NONE;

    if (drop_data.index() < owner_->VisibleBrowserActions())
      return ui::DragDropTypes::DRAG_NONE;
  }

  return ui::DragDropTypes::DRAG_MOVE;
}

int BrowserActionOverflowMenuController::OnPerformDrop(
    views::MenuItemView* menu,
    DropPosition position,
    const views::DropTargetEvent& event) {
  BrowserActionDragData drop_data;
  if (!drop_data.Read(event.GetData()))
    return ui::DragDropTypes::DRAG_NONE;

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
  return ui::DragDropTypes::DRAG_MOVE;
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
  return ui::DragDropTypes::DRAG_MOVE;
}

BrowserActionView* BrowserActionOverflowMenuController::ViewForId(
    int id, size_t* index) {
  // The index of the view being dragged (GetCommand gives a 1-based index into
  // the overflow menu).
  size_t view_index = owner_->VisibleBrowserActions() + id - 1;
  if (index)
    *index = view_index;
  return owner_->GetBrowserActionViewAt(view_index);
}
