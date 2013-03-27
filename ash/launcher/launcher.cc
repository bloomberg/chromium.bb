// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher.h"

#include <algorithm>
#include <cmath>

#include "ash/focus_cycler.h"
#include "ash/launcher/launcher_delegate.h"
#include "ash/launcher/launcher_model.h"
#include "ash/launcher/launcher_navigator.h"
#include "ash/launcher/launcher_view.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_properties.h"
#include "grit/ash_resources.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

Launcher::Launcher(LauncherModel* launcher_model,
                   LauncherDelegate* launcher_delegate,
                   ShelfWidget* shelf_widget)
    : launcher_view_(NULL),
      alignment_(shelf_widget->GetAlignment()),
      delegate_(launcher_delegate),
      shelf_widget_(shelf_widget) {
  launcher_view_ = new internal::LauncherView(
      launcher_model, delegate_, shelf_widget_->shelf_layout_manager());
  launcher_view_->Init();
  shelf_widget_->GetContentsView()->AddChildView(launcher_view_);
  shelf_widget_->GetNativeView()->SetName("LauncherView");
  shelf_widget_->GetNativeView()->SetProperty(
      internal::kStayInSameRootWindowKey, true);
}

Launcher::~Launcher() {
}

// static
Launcher* Launcher::ForPrimaryDisplay() {
  return internal::RootWindowController::ForLauncher(
      Shell::GetPrimaryRootWindow())->shelf()->launcher();
}

// static
Launcher* Launcher::ForWindow(aura::Window* window) {
  return internal::RootWindowController::ForLauncher(window)->
      shelf()->launcher();
}

void Launcher::SetAlignment(ShelfAlignment alignment) {
  alignment_ = alignment;
  launcher_view_->OnShelfAlignmentChanged();
  // ShelfLayoutManager will resize the launcher.
}

gfx::Rect Launcher::GetScreenBoundsOfItemIconForWindow(aura::Window* window) {
  LauncherID id = delegate_->GetIDByWindow(window);
  gfx::Rect bounds(launcher_view_->GetIdealBoundsOfItemIcon(id));
  if (bounds.IsEmpty())
    return bounds;

  gfx::Point screen_origin;
  views::View::ConvertPointToScreen(launcher_view_, &screen_origin);
  return gfx::Rect(screen_origin.x() + bounds.x(),
                   screen_origin.y() + bounds.y(),
                   bounds.width(),
                   bounds.height());
}

void Launcher::UpdateIconPositionForWindow(aura::Window* window) {
  launcher_view_->UpdatePanelIconPosition(
      delegate_->GetIDByWindow(window),
      window->bounds().CenterPoint());
}

void Launcher::ActivateLauncherItem(int index) {
  const ash::LauncherItems& items =
      launcher_view_->model()->items();
  ui::MouseEvent event(ui::ET_MOUSE_PRESSED,
                       gfx::Point(),
                       gfx::Point(),
                       ui::EF_NONE);
  delegate_->ItemClicked(items[index], event);
}

void Launcher::CycleWindowLinear(CycleDirection direction) {
  int item_index = GetNextActivatedItemIndex(
      *(launcher_view_->model()), direction);
  if (item_index >= 0)
    ActivateLauncherItem(item_index);
}

void Launcher::AddIconObserver(LauncherIconObserver* observer) {
  launcher_view_->AddIconObserver(observer);
}

void Launcher::RemoveIconObserver(LauncherIconObserver* observer) {
  launcher_view_->RemoveIconObserver(observer);
}

bool Launcher::IsShowingMenu() const {
  return launcher_view_->IsShowingMenu();
}

void Launcher::ShowContextMenu(const gfx::Point& location) {
  launcher_view_->ShowContextMenu(location, false);
}

bool Launcher::IsShowingOverflowBubble() const {
  return launcher_view_->IsShowingOverflowBubble();
}

void Launcher::SetVisible(bool visible) const {
  launcher_view_->SetVisible(visible);
}

bool Launcher::IsVisible() const {
  return launcher_view_->visible();
}

views::View* Launcher::GetAppListButtonView() const {
  return launcher_view_->GetAppListButtonView();
}

void Launcher::SwitchToWindow(int window_index) {
  LauncherModel* launcher_model = launcher_view_->model();
  const LauncherItems& items = launcher_model->items();
  int item_count = launcher_model->item_count();
  int indexes_left = window_index >= 0 ? window_index : item_count;
  int found_index = -1;

  // Iterating until we have hit the index we are interested in which
  // is true once indexes_left becomes negative.
  for (int i = 0; i < item_count && indexes_left >= 0; i++) {
    if (items[i].type != TYPE_APP_LIST &&
        items[i].type != TYPE_BROWSER_SHORTCUT) {
      found_index = i;
      indexes_left--;
    }
  }

  // There are two ways how found_index can be valid: a.) the nth item was
  // found (which is true when indexes_left is -1) or b.) the last item was
  // requested (which is true when index was passed in as a negative number).
  if (found_index >= 0 && (indexes_left == -1 || window_index < 0) &&
      (items[found_index].status == ash::STATUS_RUNNING ||
       items[found_index].status == ash::STATUS_CLOSED)) {
    // Then set this one as active.
    ActivateLauncherItem(found_index);
  }
}

internal::LauncherView* Launcher::GetLauncherViewForTest() {
  return launcher_view_;
}

void Launcher::SetLauncherViewBounds(gfx::Rect bounds) {
  launcher_view_->SetBoundsRect(bounds);
}

gfx::Rect Launcher::GetLauncherViewBounds() const {
  return launcher_view_->bounds();
}

}  // namespace ash
