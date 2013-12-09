// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher.h"

#include <algorithm>
#include <cmath>

#include "ash/focus_cycler.h"
#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shelf/shelf_delegate.h"
#include "ash/shelf/shelf_item_delegate.h"
#include "ash/shelf/shelf_item_delegate_manager.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_model.h"
#include "ash/shelf/shelf_navigator.h"
#include "ash/shelf/shelf_util.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
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

const char Launcher::kNativeViewName[] = "ShelfView";

Launcher::Launcher(ShelfModel* shelf_model,
                   ShelfDelegate* shelf_delegate,
                   ShelfWidget* shelf_widget)
    : shelf_view_(NULL),
      alignment_(shelf_widget->GetAlignment()),
      delegate_(shelf_delegate),
      shelf_widget_(shelf_widget) {
  shelf_view_ = new internal::ShelfView(
      shelf_model, delegate_, shelf_widget_->shelf_layout_manager());
  shelf_view_->Init();
  shelf_widget_->GetContentsView()->AddChildView(shelf_view_);
  shelf_widget_->GetNativeView()->SetName(kNativeViewName);
  delegate_->OnLauncherCreated(this);
}

Launcher::~Launcher() {
  delegate_->OnLauncherDestroyed(this);
}

// static
Launcher* Launcher::ForPrimaryDisplay() {
  ShelfWidget* shelf_widget = internal::RootWindowController::ForLauncher(
      Shell::GetPrimaryRootWindow())->shelf();
  return shelf_widget ? shelf_widget->launcher() : NULL;
}

// static
Launcher* Launcher::ForWindow(aura::Window* window) {
  ShelfWidget* shelf_widget =
    internal::RootWindowController::ForLauncher(window)->shelf();
  return shelf_widget ? shelf_widget->launcher() : NULL;
}

void Launcher::SetAlignment(ShelfAlignment alignment) {
  alignment_ = alignment;
  shelf_view_->OnShelfAlignmentChanged();
  // ShelfLayoutManager will resize the launcher.
}

gfx::Rect Launcher::GetScreenBoundsOfItemIconForWindow(aura::Window* window) {
  LauncherID id = GetLauncherIDForWindow(window);
  gfx::Rect bounds(shelf_view_->GetIdealBoundsOfItemIcon(id));
  gfx::Point screen_origin;
  views::View::ConvertPointToScreen(shelf_view_, &screen_origin);
  return gfx::Rect(screen_origin.x() + bounds.x(),
                   screen_origin.y() + bounds.y(),
                   bounds.width(),
                   bounds.height());
}

void Launcher::UpdateIconPositionForWindow(aura::Window* window) {
  shelf_view_->UpdatePanelIconPosition(
      GetLauncherIDForWindow(window),
      ash::ScreenAsh::ConvertRectFromScreen(
          shelf_widget()->GetNativeView(),
          window->GetBoundsInScreen()).CenterPoint());
}

void Launcher::ActivateLauncherItem(int index) {
  // We pass in a keyboard event which will then trigger a switch to the
  // next item if the current one is already active.
  ui::KeyEvent event(ui::ET_KEY_RELEASED,
                     ui::VKEY_UNKNOWN,  // The actual key gets ignored.
                     ui::EF_NONE,
                     false);

  const LauncherItem& item = shelf_view_->model()->items()[index];
  ShelfItemDelegate* item_delegate =
      Shell::GetInstance()->shelf_item_delegate_manager()->GetShelfItemDelegate(
          item.id);
  item_delegate->ItemSelected(event);
}

void Launcher::CycleWindowLinear(CycleDirection direction) {
  int item_index = GetNextActivatedItemIndex(
      *(shelf_view_->model()), direction);
  if (item_index >= 0)
    ActivateLauncherItem(item_index);
}

void Launcher::AddIconObserver(ShelfIconObserver* observer) {
  shelf_view_->AddIconObserver(observer);
}

void Launcher::RemoveIconObserver(ShelfIconObserver* observer) {
  shelf_view_->RemoveIconObserver(observer);
}

bool Launcher::IsShowingMenu() const {
  return shelf_view_->IsShowingMenu();
}

bool Launcher::IsShowingOverflowBubble() const {
  return shelf_view_->IsShowingOverflowBubble();
}

void Launcher::SetVisible(bool visible) const {
  shelf_view_->SetVisible(visible);
}

bool Launcher::IsVisible() const {
  return shelf_view_->visible();
}

void Launcher::SchedulePaint() {
  shelf_view_->SchedulePaintForAllButtons();
}

views::View* Launcher::GetAppListButtonView() const {
  return shelf_view_->GetAppListButtonView();
}

void Launcher::LaunchAppIndexAt(int item_index) {
  ShelfModel* shelf_model = shelf_view_->model();
  const LauncherItems& items = shelf_model->items();
  int item_count = shelf_model->item_count();
  int indexes_left = item_index >= 0 ? item_index : item_count;
  int found_index = -1;

  // Iterating until we have hit the index we are interested in which
  // is true once indexes_left becomes negative.
  for (int i = 0; i < item_count && indexes_left >= 0; i++) {
    if (items[i].type != TYPE_APP_LIST) {
      found_index = i;
      indexes_left--;
    }
  }

  // There are two ways how found_index can be valid: a.) the nth item was
  // found (which is true when indexes_left is -1) or b.) the last item was
  // requested (which is true when index was passed in as a negative number).
  if (found_index >= 0 && (indexes_left == -1 || item_index < 0)) {
    // Then set this one as active (or advance to the next item of its kind).
    ActivateLauncherItem(found_index);
  }
}

void Launcher::SetShelfViewBounds(gfx::Rect bounds) {
  shelf_view_->SetBoundsRect(bounds);
}

gfx::Rect Launcher::GetShelfViewBounds() const {
  return shelf_view_->bounds();
}

gfx::Rect Launcher::GetVisibleItemsBoundsInScreen() const {
  return shelf_view_->GetVisibleItemsBoundsInScreen();
}

app_list::ApplicationDragAndDropHost* Launcher::GetDragAndDropHostForAppList() {
  return shelf_view_;
}

}  // namespace ash
