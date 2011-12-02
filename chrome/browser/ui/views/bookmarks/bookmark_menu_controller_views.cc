// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_menu_controller_views.h"

#include "base/stl_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_menu_delegate.h"
#include "chrome/browser/ui/views/event_utils.h"
#include "chrome/common/pref_names.h"
#include "content/browser/tab_contents/page_navigator.h"
#include "content/browser/user_metrics.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

using views::MenuItemView;

BookmarkMenuController::BookmarkMenuController(Profile* profile,
                                               PageNavigator* page_navigator,
                                               views::Widget* parent,
                                               const BookmarkNode* node,
                                               int start_child_index)
    : menu_delegate_(
        new BookmarkMenuDelegate(profile, page_navigator, parent, 1)),
      node_(node),
      observer_(NULL),
      for_drop_(false),
      bookmark_bar_(NULL) {
  menu_delegate_->Init(this, NULL, node, start_child_index,
                       BookmarkMenuDelegate::HIDE_PERMANENT_FOLDERS,
                       bookmark_utils::LAUNCH_BAR_SUBFOLDER);
  menu_runner_.reset(new views::MenuRunner(menu_delegate_->menu()));
}

void BookmarkMenuController::RunMenuAt(BookmarkBarView* bookmark_bar,
                                       bool for_drop) {
  bookmark_bar_ = bookmark_bar;
  views::MenuButton* menu_button = bookmark_bar_->GetMenuButtonForNode(node_);
  DCHECK(menu_button);
  MenuItemView::AnchorPosition anchor;
  bookmark_bar_->GetAnchorPositionForButton(menu_button, &anchor);
  gfx::Point screen_loc;
  views::View::ConvertPointToScreen(menu_button, &screen_loc);
  // Subtract 1 from the height to make the popup flush with the button border.
  gfx::Rect bounds(screen_loc.x(), screen_loc.y(), menu_button->width(),
                   menu_button->height() - 1);
  for_drop_ = for_drop;
  menu_delegate_->profile()->GetBookmarkModel()->AddObserver(this);
  // We only delete ourself after the menu completes, so we can safely ignore
  // the return value.
  ignore_result(menu_runner_->RunMenuAt(menu_delegate_->parent(), menu_button,
      bounds, anchor, for_drop ? views::MenuRunner::FOR_DROP : 0));
  if (!for_drop)
    delete this;
}

void BookmarkMenuController::Cancel() {
  menu_delegate_->menu()->Cancel();
}

MenuItemView* BookmarkMenuController::menu() const {
  return menu_delegate_->menu();
}

MenuItemView* BookmarkMenuController::context_menu() const {
  return menu_delegate_->context_menu();
}

void BookmarkMenuController::SetPageNavigator(PageNavigator* navigator) {
  menu_delegate_->SetPageNavigator(navigator);
}

string16 BookmarkMenuController::GetTooltipText(int id,
                                                const gfx::Point& p) const {
  return menu_delegate_->GetTooltipText(id, p);
}

bool BookmarkMenuController::IsTriggerableEvent(views::MenuItemView* menu,
                                                const views::MouseEvent& e) {
  return menu_delegate_->IsTriggerableEvent(menu, e);
}

void BookmarkMenuController::ExecuteCommand(int id, int mouse_event_flags) {
  menu_delegate_->ExecuteCommand(id, mouse_event_flags);
}

bool BookmarkMenuController::GetDropFormats(
      MenuItemView* menu,
      int* formats,
      std::set<ui::OSExchangeData::CustomFormat>* custom_formats) {
  return menu_delegate_->GetDropFormats(menu, formats, custom_formats);
}

bool BookmarkMenuController::AreDropTypesRequired(MenuItemView* menu) {
  return menu_delegate_->AreDropTypesRequired(menu);
}

bool BookmarkMenuController::CanDrop(MenuItemView* menu,
                                     const ui::OSExchangeData& data) {
  return menu_delegate_->CanDrop(menu, data);
}

int BookmarkMenuController::GetDropOperation(
    MenuItemView* item,
    const views::DropTargetEvent& event,
    DropPosition* position) {
  return menu_delegate_->GetDropOperation(item, event, position);
}

int BookmarkMenuController::OnPerformDrop(MenuItemView* menu,
                                          DropPosition position,
                                          const views::DropTargetEvent& event) {
  int result = menu_delegate_->OnPerformDrop(menu, position, event);
  if (for_drop_)
    delete this;
  return result;
}

bool BookmarkMenuController::ShowContextMenu(MenuItemView* source,
                                             int id,
                                             const gfx::Point& p,
                                             bool is_mouse_gesture) {
  return menu_delegate_->ShowContextMenu(source, id, p, is_mouse_gesture);
}

void BookmarkMenuController::DropMenuClosed(MenuItemView* menu) {
  delete this;
}

bool BookmarkMenuController::CanDrag(MenuItemView* menu) {
  return menu_delegate_->CanDrag(menu);
}

void BookmarkMenuController::WriteDragData(MenuItemView* sender,
                                           ui::OSExchangeData* data) {
  return menu_delegate_->WriteDragData(sender, data);
}

int BookmarkMenuController::GetDragOperations(MenuItemView* sender) {
  return menu_delegate_->GetDragOperations(sender);
}

views::MenuItemView* BookmarkMenuController::GetSiblingMenu(
    views::MenuItemView* menu,
    const gfx::Point& screen_point,
    views::MenuItemView::AnchorPosition* anchor,
    bool* has_mnemonics,
    views::MenuButton** button) {
  if (!bookmark_bar_ || for_drop_)
    return NULL;
  gfx::Point bookmark_bar_loc(screen_point);
  views::View::ConvertPointToView(NULL, bookmark_bar_, &bookmark_bar_loc);
  int start_index;
  const BookmarkNode* node = bookmark_bar_->GetNodeForButtonAtModelIndex(
      bookmark_bar_loc, &start_index);
  if (!node || !node->is_folder())
    return NULL;

  menu_delegate_->SetActiveMenu(node, start_index);
  *button = bookmark_bar_->GetMenuButtonForNode(node);
  bookmark_bar_->GetAnchorPositionForButton(*button, anchor);
  *has_mnemonics = false;
  return this->menu();
}

int BookmarkMenuController::GetMaxWidthForMenu(MenuItemView* view) {
  return menu_delegate_->GetMaxWidthForMenu(view);
}

void BookmarkMenuController::BookmarkModelChanged() {
  if (!menu_delegate_->is_mutating_model())
    menu()->Cancel();
}

BookmarkMenuController::~BookmarkMenuController() {
  menu_delegate_->profile()->GetBookmarkModel()->RemoveObserver(this);
  if (observer_)
    observer_->BookmarkMenuDeleted(this);
}
