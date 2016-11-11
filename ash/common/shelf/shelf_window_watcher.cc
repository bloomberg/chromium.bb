// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/shelf_window_watcher.h"

#include <memory>
#include <utility>

#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/shelf/shelf_model.h"
#include "ash/common/shelf/shelf_window_watcher_item_delegate.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/common/wm_window_property.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/resources/grit/ui_resources.h"

namespace ash {
namespace {

// Update the ShelfItem from relevant window properties.
void UpdateShelfItemForWindow(ShelfItem* item, WmWindow* window) {
  item->type = static_cast<ShelfItemType>(
      window->GetIntProperty(WmWindowProperty::SHELF_ITEM_TYPE));

  item->status = STATUS_RUNNING;
  if (window->IsActive())
    item->status = STATUS_ACTIVE;
  else if (window->GetBoolProperty(WmWindowProperty::DRAW_ATTENTION))
    item->status = STATUS_ATTENTION;

  item->app_id = window->GetStringProperty(WmWindowProperty::APP_ID);

  // Prefer app icons over window icons, they're typically larger.
  gfx::ImageSkia image = window->GetAppIcon();
  if (image.isNull())
    image = window->GetWindowIcon();
  if (image.isNull()) {
    int icon = window->GetIntProperty(WmWindowProperty::SHELF_ICON_RESOURCE_ID);
    if (icon != kInvalidImageResourceID)
      image = *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(icon);
  }
  item->image = image;
}

}  // namespace

ShelfWindowWatcher::ContainerWindowObserver::ContainerWindowObserver(
    ShelfWindowWatcher* window_watcher)
    : window_watcher_(window_watcher) {}

ShelfWindowWatcher::ContainerWindowObserver::~ContainerWindowObserver() {}

void ShelfWindowWatcher::ContainerWindowObserver::OnWindowTreeChanged(
    WmWindow* window,
    const TreeChangeParams& params) {
  if (!params.old_parent && params.new_parent &&
      (params.new_parent->GetShellWindowId() ==
           kShellWindowId_DefaultContainer ||
       params.new_parent->GetShellWindowId() ==
           kShellWindowId_PanelContainer)) {
    // A new window was created in the default container or the panel container.
    window_watcher_->OnUserWindowAdded(params.target);
  }
}

void ShelfWindowWatcher::ContainerWindowObserver::OnWindowDestroying(
    WmWindow* window) {
  window_watcher_->OnContainerWindowDestroying(window);
}

////////////////////////////////////////////////////////////////////////////////

ShelfWindowWatcher::UserWindowObserver::UserWindowObserver(
    ShelfWindowWatcher* window_watcher)
    : window_watcher_(window_watcher) {}

ShelfWindowWatcher::UserWindowObserver::~UserWindowObserver() {}

void ShelfWindowWatcher::UserWindowObserver::OnWindowPropertyChanged(
    WmWindow* window,
    WmWindowProperty property) {
  if (property == WmWindowProperty::APP_ICON ||
      property == WmWindowProperty::APP_ID ||
      property == WmWindowProperty::DRAW_ATTENTION ||
      property == WmWindowProperty::SHELF_ITEM_TYPE ||
      property == WmWindowProperty::SHELF_ICON_RESOURCE_ID ||
      property == WmWindowProperty::WINDOW_ICON) {
    window_watcher_->OnUserWindowPropertyChanged(window);
  }
}

void ShelfWindowWatcher::UserWindowObserver::OnWindowDestroying(
    WmWindow* window) {
  window_watcher_->OnUserWindowDestroying(window);
}

////////////////////////////////////////////////////////////////////////////////

ShelfWindowWatcher::ShelfWindowWatcher(ShelfModel* model)
    : model_(model),
      container_window_observer_(this),
      user_window_observer_(this),
      observed_container_windows_(&container_window_observer_),
      observed_user_windows_(&user_window_observer_) {
  WmShell::Get()->AddActivationObserver(this);
  for (WmWindow* root : WmShell::Get()->GetAllRootWindows()) {
    observed_container_windows_.Add(
        root->GetChildByShellWindowId(kShellWindowId_DefaultContainer));
    observed_container_windows_.Add(
        root->GetChildByShellWindowId(kShellWindowId_PanelContainer));
  }

  display::Screen::GetScreen()->AddObserver(this);
}

ShelfWindowWatcher::~ShelfWindowWatcher() {
  display::Screen::GetScreen()->RemoveObserver(this);
  WmShell::Get()->RemoveActivationObserver(this);
}

void ShelfWindowWatcher::AddShelfItem(WmWindow* window) {
  user_windows_with_items_.insert(window);
  ShelfItem item;
  ShelfID id = model_->next_id();
  UpdateShelfItemForWindow(&item, window);
  window->SetIntProperty(WmWindowProperty::SHELF_ID, id);
  std::unique_ptr<ShelfItemDelegate> item_delegate(
      new ShelfWindowWatcherItemDelegate(id, window));
  model_->SetShelfItemDelegate(id, std::move(item_delegate));
  // Panels are inserted on the left so as not to push all existing panels over.
  model_->AddAt(item.type == TYPE_APP_PANEL ? 0 : model_->item_count(), item);
}

void ShelfWindowWatcher::RemoveShelfItem(WmWindow* window) {
  user_windows_with_items_.erase(window);
  int shelf_id = window->GetIntProperty(WmWindowProperty::SHELF_ID);
  DCHECK_NE(shelf_id, kInvalidShelfID);
  int index = model_->ItemIndexByID(shelf_id);
  DCHECK_GE(index, 0);
  model_->RemoveItemAt(index);
  window->SetIntProperty(WmWindowProperty::SHELF_ID, kInvalidShelfID);
}

void ShelfWindowWatcher::OnContainerWindowDestroying(WmWindow* container) {
  observed_container_windows_.Remove(container);
}

int ShelfWindowWatcher::GetShelfItemIndexForWindow(WmWindow* window) const {
  return model_->ItemIndexByID(
      window->GetIntProperty(WmWindowProperty::SHELF_ID));
}

void ShelfWindowWatcher::OnUserWindowAdded(WmWindow* window) {
  // The window may already be tracked from a prior display or parent container.
  if (observed_user_windows_.IsObserving(window))
    return;

  observed_user_windows_.Add(window);

  // Add, update, or remove a ShelfItem for |window|, as needed.
  OnUserWindowPropertyChanged(window);
}

void ShelfWindowWatcher::OnUserWindowDestroying(WmWindow* window) {
  if (observed_user_windows_.IsObserving(window))
    observed_user_windows_.Remove(window);

  if (user_windows_with_items_.count(window) > 0)
    RemoveShelfItem(window);
  DCHECK_EQ(0u, user_windows_with_items_.count(window));
}

void ShelfWindowWatcher::OnUserWindowPropertyChanged(WmWindow* window) {
  if (window->GetIntProperty(WmWindowProperty::SHELF_ITEM_TYPE) ==
      TYPE_UNDEFINED) {
    // Remove |window|'s ShelfItem if it was added by this ShelfWindowWatcher.
    if (user_windows_with_items_.count(window) > 0)
      RemoveShelfItem(window);
    return;
  }

  // Update an existing ShelfItem for |window| when a property has changed.
  int index = GetShelfItemIndexForWindow(window);
  if (index > 0) {
    ShelfItem item = model_->items()[index];
    UpdateShelfItemForWindow(&item, window);
    model_->Set(index, item);
    return;
  }

  // Creates a new ShelfItem for |window|.
  AddShelfItem(window);
}

void ShelfWindowWatcher::OnWindowActivated(WmWindow* gained_active,
                                           WmWindow* lost_active) {
  if (gained_active && user_windows_with_items_.count(gained_active) > 0)
    OnUserWindowPropertyChanged(gained_active);
  if (lost_active && user_windows_with_items_.count(lost_active) > 0)
    OnUserWindowPropertyChanged(lost_active);
}

void ShelfWindowWatcher::OnDisplayAdded(const display::Display& new_display) {
  WmWindow* root = WmShell::Get()->GetRootWindowForDisplayId(new_display.id());

  // When the primary root window's display get removed, the existing root
  // window is taken over by the new display and the observer is already set.
  WmWindow* default_container =
      root->GetChildByShellWindowId(kShellWindowId_DefaultContainer);
  if (!observed_container_windows_.IsObserving(default_container))
    observed_container_windows_.Add(default_container);
  WmWindow* panel_container =
      root->GetChildByShellWindowId(kShellWindowId_PanelContainer);
  if (!observed_container_windows_.IsObserving(panel_container))
    observed_container_windows_.Add(panel_container);
}

void ShelfWindowWatcher::OnDisplayRemoved(const display::Display& old_display) {
}

void ShelfWindowWatcher::OnDisplayMetricsChanged(const display::Display&,
                                                 uint32_t) {}

}  // namespace ash
