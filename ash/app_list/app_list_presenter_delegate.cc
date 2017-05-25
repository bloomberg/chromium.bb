// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_presenter_delegate.h"

#include "ash/ash_switches.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shelf/app_list_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/wm_window.h"
#include "base/command_line.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/presenter/app_list_presenter_impl.h"
#include "ui/app_list/presenter/app_list_view_delegate_factory.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Gets the point at the center of the display containing the given |window|.
// This calculation excludes the virtual keyboard area. If the height of the
// display area is less than |minimum_height|, its bottom will be extended to
// that height (so that the app list never starts above the top of the screen).
gfx::Point GetCenterOfDisplayForWindow(WmWindow* window, int minimum_height) {
  DCHECK(window);
  gfx::Rect bounds =
      ScreenUtil::GetDisplayBoundsWithShelf(window->aura_window());
  bounds = window->GetRootWindow()->ConvertRectToScreen(bounds);

  // If the virtual keyboard is active, subtract it from the display bounds, so
  // that the app list is centered in the non-keyboard area of the display.
  // (Note that work_area excludes the keyboard, but it doesn't get updated
  // until after this function is called.)
  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  if (keyboard_controller && keyboard_controller->keyboard_visible())
    bounds.Subtract(keyboard_controller->current_keyboard_bounds());

  // Apply the |minimum_height|.
  if (bounds.height() < minimum_height)
    bounds.set_height(minimum_height);

  return bounds.CenterPoint();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterDelegate, public:

AppListPresenterDelegate::AppListPresenterDelegate(
    app_list::AppListPresenterImpl* presenter,
    app_list::AppListViewDelegateFactory* view_delegate_factory)
    : presenter_(presenter), view_delegate_factory_(view_delegate_factory) {
  Shell::Get()->AddShellObserver(this);
}

AppListPresenterDelegate::~AppListPresenterDelegate() {
  DCHECK(view_);
  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  if (keyboard_controller)
    keyboard_controller->RemoveObserver(this);
  Shell::Get()->RemovePreTargetHandler(this);
  WmWindow* window = WmWindow::Get(view_->GetWidget()->GetNativeWindow());
  window->GetRootWindowController()->GetShelf()->RemoveObserver(this);
  Shell::Get()->RemoveShellObserver(this);
}

app_list::AppListViewDelegate* AppListPresenterDelegate::GetViewDelegate() {
  return view_delegate_factory_->GetDelegate();
}

void AppListPresenterDelegate::Init(app_list::AppListView* view,
                                    int64_t display_id,
                                    int current_apps_page) {
  // App list needs to know the new shelf layout in order to calculate its
  // UI layout when AppListView visibility changes.
  ash::Shell::GetPrimaryRootWindowController()
      ->GetShelfLayoutManager()
      ->UpdateAutoHideState();
  view_ = view;
  aura::Window* root_window =
      ShellPort::Get()->GetRootWindowForDisplayId(display_id);
  aura::Window* container = GetRootWindowController(root_window)
                                ->GetContainer(kShellWindowId_AppListContainer);

  view->Initialize(container, current_apps_page);

  if (!app_list::features::IsFullscreenAppListEnabled()) {
    view->MaybeSetAnchorPoint(GetCenterOfDisplayForWindow(
        WmWindow::Get(root_window), GetMinimumBoundsHeightForAppList(view)));
  }
  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  if (keyboard_controller)
    keyboard_controller->AddObserver(this);
  Shell::Get()->AddPreTargetHandler(this);
  Shelf* shelf = Shelf::ForWindow(root_window);
  shelf->AddObserver(this);

  // By setting us as DnD recipient, the app list knows that we can
  // handle items.
  view->SetDragAndDropHostOfCurrentAppList(
      shelf->shelf_widget()->GetDragAndDropHostForAppList());
}

void AppListPresenterDelegate::OnShown(int64_t display_id) {
  is_visible_ = true;
  // Update applist button status when app list visibility is changed.
  aura::Window* root_window =
      ShellPort::Get()->GetRootWindowForDisplayId(display_id);
  AppListButton* app_list_button =
      Shelf::ForWindow(root_window)->shelf_widget()->GetAppListButton();
  if (app_list_button)
    app_list_button->OnAppListShown();
}

void AppListPresenterDelegate::OnDismissed() {
  DCHECK(is_visible_);
  DCHECK(view_);

  is_visible_ = false;

  // Update applist button status when app list visibility is changed.
  Shelf* shelf = Shelf::ForWindow(view_->GetWidget()->GetNativeWindow());
  AppListButton* app_list_button = shelf->shelf_widget()->GetAppListButton();
  if (app_list_button)
    app_list_button->OnAppListDismissed();
}

void AppListPresenterDelegate::UpdateBounds() {
  if (!view_ || !is_visible_)
    return;

  view_->UpdateBounds();
  view_->MaybeSetAnchorPoint(GetCenterOfDisplayForWindow(
      WmWindow::Get(view_->GetWidget()->GetNativeWindow()),
      GetMinimumBoundsHeightForAppList(view_)));
}

gfx::Vector2d AppListPresenterDelegate::GetVisibilityAnimationOffset(
    aura::Window* root_window) {
  DCHECK(Shell::HasInstance());

  // App list needs to know the new shelf layout in order to calculate its
  // UI layout when AppListView visibility changes.
  Shelf* shelf = Shelf::ForWindow(root_window);
  shelf->UpdateAutoHideState();

  switch (shelf->alignment()) {
    case SHELF_ALIGNMENT_BOTTOM:
    case SHELF_ALIGNMENT_BOTTOM_LOCKED:
      return gfx::Vector2d(0, kAnimationOffset);
    case SHELF_ALIGNMENT_LEFT:
      return gfx::Vector2d(-kAnimationOffset, 0);
    case SHELF_ALIGNMENT_RIGHT:
      return gfx::Vector2d(kAnimationOffset, 0);
  }
  NOTREACHED();
  return gfx::Vector2d();
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterDelegate, private:

void AppListPresenterDelegate::ProcessLocatedEvent(ui::LocatedEvent* event) {
  if (!view_ || !is_visible_)
    return;

  // If the event happened on a menu, then the event should not close the app
  // list.
  aura::Window* target = static_cast<aura::Window*>(event->target());
  if (target) {
    RootWindowController* root_controller =
        GetRootWindowController(target->GetRootWindow());
    if (root_controller) {
      aura::Window* menu_container =
          root_controller->GetContainer(kShellWindowId_MenuContainer);
      if (menu_container->Contains(target))
        return;
      aura::Window* keyboard_container = root_controller->GetContainer(
          kShellWindowId_VirtualKeyboardContainer);
      if (keyboard_container->Contains(target))
        return;
    }
  }

  aura::Window* window = view_->GetWidget()->GetNativeView()->parent();
  if (!window->Contains(target) &&
      !app_list::switches::ShouldNotDismissOnBlur()) {
    presenter_->Dismiss();
  }
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterDelegate, aura::EventFilter implementation:

void AppListPresenterDelegate::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED)
    ProcessLocatedEvent(event);
}

void AppListPresenterDelegate::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP_DOWN)
    ProcessLocatedEvent(event);
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterDelegate, keyboard::KeyboardControllerObserver
// implementation:

void AppListPresenterDelegate::OnKeyboardBoundsChanging(
    const gfx::Rect& new_bounds) {
  UpdateBounds();
}

void AppListPresenterDelegate::OnKeyboardClosed() {}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterDelegate, ShellObserver implementation:
void AppListPresenterDelegate::OnOverviewModeStarting() {
  if (is_visible_)
    presenter_->Dismiss();
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterDelegate, ShelfObserver implementation:

void AppListPresenterDelegate::OnShelfIconPositionsChanged() {
  UpdateBounds();
}

}  // namespace ash
