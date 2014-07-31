// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/browser_action_overflow_menu_controller.h"

#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/extensions/browser_action_drag_data.h"
#include "chrome/browser/ui/views/toolbar/browser_action_view.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "extensions/common/extension.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/widget/widget.h"

// In the browser actions container's chevron menu, a menu item view's icon
// comes from BrowserActionView::GetIconWithBadge() (which comes from the
// browser action button's icon) when the menu item view is created. But, the
// browser action button's icon may not be loaded in time because it is read
// from file system in another thread.
// The IconUpdater will update the menu item view's icon when the browser
// action button's icon has been updated.
class IconUpdater : public BrowserActionButton::IconObserver {
 public:
  IconUpdater(views::MenuItemView* menu_item_view, BrowserActionButton* button)
      : menu_item_view_(menu_item_view),
        button_(button) {
    DCHECK(menu_item_view);
    DCHECK(button);
    button->set_icon_observer(this);
  }
  virtual ~IconUpdater() {
    button_->set_icon_observer(NULL);
  }

  // Overridden from BrowserActionButton::IconObserver:
  virtual void OnIconUpdated(const gfx::ImageSkia& icon) OVERRIDE {
    menu_item_view_->SetIcon(icon);
  }

 private:
  // The menu item view whose icon might be updated.
  views::MenuItemView* menu_item_view_;

  // The button to be observed. When its icon changes, update the corresponding
  // menu item view's icon.
  BrowserActionButton* button_;

  DISALLOW_COPY_AND_ASSIGN(IconUpdater);
};

BrowserActionOverflowMenuController::BrowserActionOverflowMenuController(
    BrowserActionsContainer* owner,
    Browser* browser,
    views::MenuButton* menu_button,
    const std::vector<BrowserActionView*>& views,
    int start_index,
    bool for_drop)
    : owner_(owner),
      browser_(browser),
      observer_(NULL),
      menu_button_(menu_button),
      menu_(NULL),
      views_(&views),
      start_index_(start_index),
      for_drop_(for_drop) {
  menu_ = new views::MenuItemView(this);
  menu_runner_.reset(new views::MenuRunner(
      menu_, for_drop_ ? views::MenuRunner::FOR_DROP : 0));
  menu_->set_has_icons(true);

  size_t command_id = 1;  // Menu id 0 is reserved, start with 1.
  for (size_t i = start_index; i < views_->size(); ++i) {
    BrowserActionView* view = (*views_)[i];
    views::MenuItemView* menu_item = menu_->AppendMenuItemWithIcon(
        command_id,
        base::UTF8ToUTF16(view->button()->extension()->name()),
        view->GetIconWithBadge());

    // Set the tooltip for this item.
    base::string16 tooltip = base::UTF8ToUTF16(
        extensions::ExtensionActionManager::Get(owner_->profile())->
        GetBrowserAction(*view->button()->extension())->
        GetTitle(owner_->GetCurrentTabId()));
    menu_->SetTooltip(tooltip, command_id);

    icon_updaters_.push_back(new IconUpdater(menu_item, view->button()));

    ++command_id;
  }
}

BrowserActionOverflowMenuController::~BrowserActionOverflowMenuController() {
  if (observer_)
    observer_->NotifyMenuDeleted(this);
}

bool BrowserActionOverflowMenuController::RunMenu(views::Widget* window) {
  gfx::Rect bounds = menu_button_->bounds();
  gfx::Point screen_loc;
  views::View::ConvertPointToScreen(menu_button_, &screen_loc);
  bounds.set_x(screen_loc.x());
  bounds.set_y(screen_loc.y());

  views::MenuAnchorPosition anchor = views::MENU_ANCHOR_TOPRIGHT;
  // As we maintain our own lifetime we can safely ignore the result.
  ignore_result(menu_runner_->RunMenuAt(
      window, menu_button_, bounds, anchor, ui::MENU_SOURCE_NONE));
  if (!for_drop_) {
    // Give the context menu (if any) a chance to execute the user-selected
    // command.
    base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }
  return true;
}

void BrowserActionOverflowMenuController::CancelMenu() {
  menu_->Cancel();
}

void BrowserActionOverflowMenuController::NotifyBrowserActionViewsDeleting() {
  icon_updaters_.clear();
}

bool BrowserActionOverflowMenuController::IsCommandEnabled(int id) const {
  BrowserActionView* view = (*views_)[start_index_ + id - 1];
  return view->button()->IsEnabled(owner_->GetCurrentTabId());
}

void BrowserActionOverflowMenuController::ExecuteCommand(int id) {
  BrowserActionView* view = (*views_)[start_index_ + id - 1];
  view->button()->ExecuteBrowserAction();
}

bool BrowserActionOverflowMenuController::ShowContextMenu(
    views::MenuItemView* source,
    int id,
    const gfx::Point& p,
    ui::MenuSourceType source_type) {
  BrowserActionButton* button = (*views_)[start_index_ + id - 1]->button();
  if (!button->extension()->ShowConfigureContextMenus())
    return false;

  scoped_refptr<ExtensionContextMenuModel> context_menu_contents =
      new ExtensionContextMenuModel(button->extension(), browser_, button);
  views::MenuRunner context_menu_runner(context_menu_contents.get(),
                                        views::MenuRunner::HAS_MNEMONICS |
                                            views::MenuRunner::IS_NESTED |
                                            views::MenuRunner::CONTEXT_MENU);

  // We can ignore the result as we delete ourself.
  // This blocks until the user choses something or dismisses the menu.
  ignore_result(context_menu_runner.RunMenuAt(menu_button_->GetWidget(),
                                              NULL,
                                              gfx::Rect(p, gfx::Size()),
                                              views::MENU_ANCHOR_TOPLEFT,
                                              source_type));

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
  return BrowserActionDragData::GetDropFormats(custom_formats);
}

bool BrowserActionOverflowMenuController::AreDropTypesRequired(
    views::MenuItemView* menu) {
  return BrowserActionDragData::AreDropTypesRequired();
}

bool BrowserActionOverflowMenuController::CanDrop(
    views::MenuItemView* menu, const OSExchangeData& data) {
  return BrowserActionDragData::CanDrop(data, owner_->profile());
}

int BrowserActionOverflowMenuController::GetDropOperation(
    views::MenuItemView* item,
    const ui::DropTargetEvent& event,
    DropPosition* position) {
  // Don't allow dropping from the BrowserActionContainer into slot 0 of the
  // overflow menu since once the move has taken place the item you are dragging
  // falls right out of the menu again once the user releases the button
  // (because we don't shrink the BrowserActionContainer when you do this).
  if ((item->GetCommand() == 0) && (*position == DROP_BEFORE)) {
    BrowserActionDragData drop_data;
    if (!drop_data.Read(event.data()))
      return ui::DragDropTypes::DRAG_NONE;

    if (drop_data.index() < owner_->VisibleBrowserActions())
      return ui::DragDropTypes::DRAG_NONE;
  }

  return ui::DragDropTypes::DRAG_MOVE;
}

int BrowserActionOverflowMenuController::OnPerformDrop(
    views::MenuItemView* menu,
    DropPosition position,
    const ui::DropTargetEvent& event) {
  BrowserActionDragData drop_data;
  if (!drop_data.Read(event.data()))
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
