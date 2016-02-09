// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/chevron_menu_button.h"

#include <stddef.h>

#include "base/location.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_icon_factory.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/browser_action_drag_data.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view.h"
#include "extensions/common/extension.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/metrics.h"

namespace {

// In the browser actions container's chevron menu, a menu item view's icon
// comes from ToolbarActionView::GetIconWithBadge() when the menu item view is
// created. But, the browser action's icon may not be loaded in time because it
// is read from file system in another thread.
// The IconUpdater will update the menu item view's icon when the browser
// action's icon has been updated.
class IconUpdater : public ExtensionActionIconFactory::Observer {
 public:
  IconUpdater(views::MenuItemView* menu_item_view,
              ToolbarActionView* represented_view)
      : menu_item_view_(menu_item_view),
        represented_view_(represented_view) {
    DCHECK(menu_item_view);
    DCHECK(represented_view);
    view_controller()->set_icon_observer(this);
  }
  ~IconUpdater() override { view_controller()->set_icon_observer(nullptr); }

  // ExtensionActionIconFactory::Observer:
  void OnIconUpdated() override {
    menu_item_view_->SetIcon(
        represented_view_->GetImage(views::Button::STATE_NORMAL));
  }

 private:
  ExtensionActionViewController* view_controller() {
    // Since the chevron overflow menu is only used in a world where toolbar
    // actions are only extensions, this cast is safe.
    return static_cast<ExtensionActionViewController*>(
        represented_view_->view_controller());
  }

  // The menu item view whose icon might be updated.
  views::MenuItemView* menu_item_view_;

  // The view this icon updater is helping represent in the chevron overflow
  // menu. When its icon changes, this updates the corresponding menu item
  // view's icon.
  ToolbarActionView* represented_view_;

  DISALLOW_COPY_AND_ASSIGN(IconUpdater);
};

}  // namespace

// This class handles the overflow menu for browser actions.
class ChevronMenuButton::MenuController : public views::MenuDelegate {
 public:
  MenuController(ChevronMenuButton* owner,
                 BrowserActionsContainer* browser_actions_container,
                 bool for_drop);
  ~MenuController() override;

  // Shows the overflow menu.
  void RunMenu(views::Widget* widget);

  // Closes the overflow menu (and its context menu if open as well).
  void CloseMenu();

 private:
  // views::MenuDelegate:
  bool IsCommandEnabled(int id) const override;
  void ExecuteCommand(int id) override;
  bool ShowContextMenu(views::MenuItemView* source,
                       int id,
                       const gfx::Point& p,
                       ui::MenuSourceType source_type) override;
  // These drag functions offer support for dragging icons into the overflow
  // menu.
  bool GetDropFormats(
      views::MenuItemView* menu,
      int* formats,
      std::set<ui::Clipboard::FormatType>* format_types) override;
  bool AreDropTypesRequired(views::MenuItemView* menu) override;
  bool CanDrop(views::MenuItemView* menu,
               const ui::OSExchangeData& data) override;
  int GetDropOperation(views::MenuItemView* item,
                       const ui::DropTargetEvent& event,
                       DropPosition* position) override;
  int OnPerformDrop(views::MenuItemView* menu,
                    DropPosition position,
                    const ui::DropTargetEvent& event) override;
  void OnMenuClosed(views::MenuItemView* menu,
                    views::MenuRunner::RunResult result) override;
  // These three drag functions offer support for dragging icons out of the
  // overflow menu.
  bool CanDrag(views::MenuItemView* menu) override;
  void WriteDragData(views::MenuItemView* sender,
                     ui::OSExchangeData* data) override;
  int GetDragOperations(views::MenuItemView* sender) override;

  // Returns the offset into |views_| for the given |id|.
  size_t IndexForId(int id) const;

  // The owning ChevronMenuButton.
  ChevronMenuButton* owner_;

  // A pointer to the browser action container.
  BrowserActionsContainer* browser_actions_container_;

  // The overflow menu for the menu button. Owned by |menu_runner_|.
  views::MenuItemView* menu_;

  // Resposible for running the menu.
  scoped_ptr<views::MenuRunner> menu_runner_;

  // The index into the ToolbarActionView vector, indicating where to start
  // picking browser actions to draw.
  int start_index_;

  // Whether this controller is being used for drop.
  bool for_drop_;

  // The vector keeps all icon updaters associated with menu item views in the
  // controller. The icon updater will update the menu item view's icon when
  // the browser action view's icon has been updated.
  ScopedVector<IconUpdater> icon_updaters_;

  DISALLOW_COPY_AND_ASSIGN(MenuController);
};

ChevronMenuButton::MenuController::MenuController(
    ChevronMenuButton* owner,
    BrowserActionsContainer* browser_actions_container,
    bool for_drop)
    : owner_(owner),
      browser_actions_container_(browser_actions_container),
      menu_(NULL),
      start_index_(
          browser_actions_container_->VisibleBrowserActionsAfterAnimation()),
      for_drop_(for_drop) {
  menu_ = new views::MenuItemView(this);
  menu_runner_.reset(new views::MenuRunner(
      menu_, for_drop_ ? views::MenuRunner::FOR_DROP : 0));
  menu_->set_has_icons(true);

  size_t command_id = 1;  // Menu id 0 is reserved, start with 1.
  for (size_t i = start_index_;
       i < browser_actions_container_->num_toolbar_actions(); ++i) {
    ToolbarActionView* view =
        browser_actions_container_->GetToolbarActionViewAt(i);
    views::MenuItemView* menu_item = menu_->AppendMenuItemWithIcon(
        command_id,
        view->view_controller()->GetActionName(),
        view->GetImage(views::Button::STATE_NORMAL));

    // Set the tooltip for this item.
    menu_->SetTooltip(
        view->view_controller()->GetTooltip(view->GetCurrentWebContents()),
        command_id);

    icon_updaters_.push_back(new IconUpdater(menu_item, view));

    ++command_id;
  }
}

ChevronMenuButton::MenuController::~MenuController() {
}

void ChevronMenuButton::MenuController::RunMenu(views::Widget* window) {
  gfx::Rect bounds = owner_->bounds();
  gfx::Point screen_loc;
  views::View::ConvertPointToScreen(owner_, &screen_loc);
  bounds.set_x(screen_loc.x());
  bounds.set_y(screen_loc.y());
  ignore_result(menu_runner_->RunMenuAt(window, owner_, bounds,
                                        views::MENU_ANCHOR_TOPRIGHT,
                                        ui::MENU_SOURCE_NONE));
}

void ChevronMenuButton::MenuController::CloseMenu() {
  icon_updaters_.clear();
  menu_->Cancel();
}

bool ChevronMenuButton::MenuController::IsCommandEnabled(int id) const {
  ToolbarActionView* view =
      browser_actions_container_->GetToolbarActionViewAt(start_index_ + id - 1);
  return view->view_controller()->IsEnabled(view->GetCurrentWebContents());
}

void ChevronMenuButton::MenuController::ExecuteCommand(int id) {
  browser_actions_container_->GetToolbarActionViewAt(start_index_ + id - 1)->
      view_controller()->ExecuteAction(true);
}

bool ChevronMenuButton::MenuController::ShowContextMenu(
    views::MenuItemView* source,
    int id,
    const gfx::Point& p,
    ui::MenuSourceType source_type) {
  ToolbarActionView* view = browser_actions_container_->GetToolbarActionViewAt(
      start_index_ + id - 1);
  ExtensionActionViewController* view_controller =
      static_cast<ExtensionActionViewController*>(view->view_controller());
  if (!view_controller->extension()->ShowConfigureContextMenus())
    return false;

  scoped_ptr<extensions::ExtensionContextMenuModel> context_menu_contents(
      new extensions::ExtensionContextMenuModel(
          view_controller->extension(), view_controller->browser(),
          extensions::ExtensionContextMenuModel::OVERFLOWED, view_controller));
  views::MenuRunner context_menu_runner(context_menu_contents.get(),
                                        views::MenuRunner::HAS_MNEMONICS |
                                            views::MenuRunner::IS_NESTED |
                                            views::MenuRunner::CONTEXT_MENU);

  // We can ignore the result as we delete ourself.
  // This blocks until the user chooses something or dismisses the menu.
  if (context_menu_runner.RunMenuAt(owner_->GetWidget(),
                                    NULL,
                                    gfx::Rect(p, gfx::Size()),
                                    views::MENU_ANCHOR_TOPLEFT,
                                    source_type) ==
          views::MenuRunner::MENU_DELETED)
    return true;

  // The user is done with the context menu, so we can close the underlying
  // menu.
  menu_->Cancel();

  return true;
}

bool ChevronMenuButton::MenuController::GetDropFormats(
    views::MenuItemView* menu,
    int* formats,
    std::set<ui::Clipboard::FormatType>* format_types) {
  return BrowserActionDragData::GetDropFormats(format_types);
}

bool ChevronMenuButton::MenuController::AreDropTypesRequired(
    views::MenuItemView* menu) {
  return BrowserActionDragData::AreDropTypesRequired();
}

bool ChevronMenuButton::MenuController::CanDrop(
    views::MenuItemView* menu, const OSExchangeData& data) {
  return BrowserActionDragData::CanDrop(
      data, browser_actions_container_->browser()->profile());
}

int ChevronMenuButton::MenuController::GetDropOperation(
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

    if (drop_data.index() < browser_actions_container_->VisibleBrowserActions())
      return ui::DragDropTypes::DRAG_NONE;
  }

  return ui::DragDropTypes::DRAG_MOVE;
}

int ChevronMenuButton::MenuController::OnPerformDrop(
    views::MenuItemView* menu,
    DropPosition position,
    const ui::DropTargetEvent& event) {
  BrowserActionDragData drop_data;
  if (!drop_data.Read(event.data()))
    return ui::DragDropTypes::DRAG_NONE;

  size_t drop_index = IndexForId(menu->GetCommand());

  // When not dragging within the overflow menu (dragging an icon into the menu)
  // subtract one to get the right index.
  if (position == DROP_BEFORE &&
      drop_data.index() < browser_actions_container_->VisibleBrowserActions())
    --drop_index;

  ToolbarActionsBar::DragType drag_type =
      drop_data.index() < browser_actions_container_->VisibleBrowserActions() ?
          ToolbarActionsBar::DRAG_TO_OVERFLOW :
          ToolbarActionsBar::DRAG_TO_SAME;
  browser_actions_container_->toolbar_actions_bar()->OnDragDrop(
      drop_data.index(), drop_index, drag_type);

  if (for_drop_)
    owner_->MenuDone();
  return ui::DragDropTypes::DRAG_MOVE;
}

void ChevronMenuButton::MenuController::OnMenuClosed(
    views::MenuItemView* menu,
    views::MenuRunner::RunResult result) {
  if (result == views::MenuRunner::MENU_DELETED)
    return;

  // Give the context menu (if any) a chance to execute the user-selected
  // command.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ChevronMenuButton::MenuDone,
                            owner_->weak_factory_.GetWeakPtr()));
}

bool ChevronMenuButton::MenuController::CanDrag(views::MenuItemView* menu) {
  return true;
}

void ChevronMenuButton::MenuController::WriteDragData(
    views::MenuItemView* sender, OSExchangeData* data) {
  size_t drag_index = IndexForId(sender->GetCommand());
  BrowserActionDragData drag_data(
      browser_actions_container_->GetIdAt(drag_index), drag_index);
  drag_data.Write(browser_actions_container_->browser()->profile(), data);
}

int ChevronMenuButton::MenuController::GetDragOperations(
    views::MenuItemView* sender) {
  return ui::DragDropTypes::DRAG_MOVE;
}

size_t ChevronMenuButton::MenuController::IndexForId(int id) const {
  // The index of the view being dragged (GetCommand gives a 1-based index into
  // the overflow menu).
  DCHECK_GT(browser_actions_container_->VisibleBrowserActions() + id, 0u);
  return browser_actions_container_->VisibleBrowserActions() + id - 1;
}

ChevronMenuButton::ChevronMenuButton(
    BrowserActionsContainer* browser_actions_container)
    : views::MenuButton(base::string16(), this, false),
      browser_actions_container_(browser_actions_container),
      weak_factory_(this) {
  // Set the border explicitly, because otherwise the native theme manager takes
  // over and reassigns the insets we set in CreateDefaultBorder().
  SetBorder(CreateDefaultBorder());
}

ChevronMenuButton::~ChevronMenuButton() {
}

void ChevronMenuButton::CloseMenu() {
  if (menu_controller_)
    menu_controller_->CloseMenu();
}

scoped_ptr<views::LabelButtonBorder> ChevronMenuButton::CreateDefaultBorder()
    const {
  // The chevron resource was designed to not have any insets.
  scoped_ptr<views::LabelButtonBorder> border =
      views::MenuButton::CreateDefaultBorder();
  border->set_insets(gfx::Insets());
  return border;
}

bool ChevronMenuButton::GetDropFormats(
    int* formats,
    std::set<ui::Clipboard::FormatType>* format_types) {
  return BrowserActionDragData::GetDropFormats(format_types);
}

bool ChevronMenuButton::AreDropTypesRequired() {
  return BrowserActionDragData::AreDropTypesRequired();
}

bool ChevronMenuButton::CanDrop(const OSExchangeData& data) {
  return BrowserActionDragData::CanDrop(
      data, browser_actions_container_->browser()->profile());
}

void ChevronMenuButton::OnDragEntered(const ui::DropTargetEvent& event) {
  DCHECK(!weak_factory_.HasWeakPtrs());
  if (!menu_controller_) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(&ChevronMenuButton::ShowOverflowMenu,
                              weak_factory_.GetWeakPtr(), true),
        base::TimeDelta::FromMilliseconds(views::GetMenuShowDelay()));
  }
}

int ChevronMenuButton::OnDragUpdated(const ui::DropTargetEvent& event) {
  return ui::DragDropTypes::DRAG_MOVE;
}

void ChevronMenuButton::OnDragExited() {
  weak_factory_.InvalidateWeakPtrs();
}

int ChevronMenuButton::OnPerformDrop(const ui::DropTargetEvent& event) {
  weak_factory_.InvalidateWeakPtrs();
  return ui::DragDropTypes::DRAG_MOVE;
}

void ChevronMenuButton::OnMenuButtonClicked(views::View* source,
                                            const gfx::Point& point) {
  DCHECK_EQ(this, source);
  // The menu could already be open if a user dragged an item over it but
  // ultimately dropped elsewhere (as in that case the menu will close on a
  // timer). In this case, the click should close the open menu.
  if (menu_controller_)
    menu_controller_->CloseMenu();
  else
    ShowOverflowMenu(false);
}

void ChevronMenuButton::ShowOverflowMenu(bool for_drop) {
  // We should never try to show an overflow menu when one is already visible.
  DCHECK(!menu_controller_);
  menu_controller_.reset(new MenuController(
      this, browser_actions_container_, for_drop));
  menu_controller_->RunMenu(GetWidget());
}

void ChevronMenuButton::MenuDone() {
  menu_controller_.reset();
}
