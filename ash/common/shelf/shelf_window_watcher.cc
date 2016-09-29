// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/shelf_window_watcher.h"

#include <memory>
#include <utility>

#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/shelf/shelf_model.h"
#include "ash/common/shelf/shelf_window_watcher_item_delegate.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/common/wm_window_property.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace {

// Update the ShelfItem from relevant window properties.
void UpdateShelfItemForWindow(ShelfItem* item, WmWindow* window) {
  item->type = static_cast<ShelfItemType>(
      window->GetIntProperty(WmWindowProperty::SHELF_ITEM_TYPE));
  const int icon =
      window->GetIntProperty(WmWindowProperty::SHELF_ICON_RESOURCE_ID);
  if (icon != kInvalidImageResourceID)
    item->image = *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(icon);
}

// Returns true if |window| has a ShelfItem added by ShelfWindowWatcher.
bool HasShelfItemForWindow(WmWindow* window) {
  return window->GetIntProperty(WmWindowProperty::SHELF_ITEM_TYPE) !=
             TYPE_UNDEFINED &&
         window->GetIntProperty(WmWindowProperty::SHELF_ID) != kInvalidShelfID;
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
      params.new_parent->GetShellWindowId() ==
          kShellWindowId_DefaultContainer) {
    // A new window was created in the default container.
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
  if (property == WmWindowProperty::SHELF_ITEM_TYPE ||
      property == WmWindowProperty::SHELF_ICON_RESOURCE_ID) {
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
  }

  display::Screen::GetScreen()->AddObserver(this);
}

ShelfWindowWatcher::~ShelfWindowWatcher() {
  display::Screen::GetScreen()->RemoveObserver(this);
  WmShell::Get()->RemoveActivationObserver(this);
}

void ShelfWindowWatcher::AddShelfItem(WmWindow* window) {
  ShelfItem item;
  ShelfID id = model_->next_id();
  item.status = window->IsActive() ? STATUS_ACTIVE : STATUS_RUNNING;
  UpdateShelfItemForWindow(&item, window);
  window->SetIntProperty(WmWindowProperty::SHELF_ID, id);
  std::unique_ptr<ShelfItemDelegate> item_delegate(
      new ShelfWindowWatcherItemDelegate(window));
  model_->SetShelfItemDelegate(id, std::move(item_delegate));
  model_->Add(item);
}

void ShelfWindowWatcher::RemoveShelfItem(WmWindow* window) {
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

void ShelfWindowWatcher::UpdateShelfItemStatus(WmWindow* window,
                                               bool is_active) {
  int index = GetShelfItemIndexForWindow(window);
  DCHECK_GE(index, 0);

  ShelfItem item = model_->items()[index];
  item.status = is_active ? STATUS_ACTIVE : STATUS_RUNNING;
  model_->Set(index, item);
}

int ShelfWindowWatcher::GetShelfItemIndexForWindow(WmWindow* window) const {
  return model_->ItemIndexByID(
      window->GetIntProperty(WmWindowProperty::SHELF_ID));
}

void ShelfWindowWatcher::OnUserWindowAdded(WmWindow* window) {
  // The window may already be tracked from when it was added to a different
  // display or because an existing window added its shelf item properties.
  if (observed_user_windows_.IsObserving(window))
    return;

  observed_user_windows_.Add(window);

  // Add a ShelfItem if |window| has a valid ShelfItemType on creation.
  if (window->GetIntProperty(WmWindowProperty::SHELF_ITEM_TYPE) !=
          TYPE_UNDEFINED &&
      window->GetIntProperty(WmWindowProperty::SHELF_ID) == kInvalidShelfID) {
    AddShelfItem(window);
  }
}

void ShelfWindowWatcher::OnUserWindowDestroying(WmWindow* window) {
  if (observed_user_windows_.IsObserving(window))
    observed_user_windows_.Remove(window);

  if (HasShelfItemForWindow(window))
    RemoveShelfItem(window);
}

void ShelfWindowWatcher::OnUserWindowPropertyChanged(WmWindow* window) {
  if (window->GetIntProperty(WmWindowProperty::SHELF_ITEM_TYPE) ==
      TYPE_UNDEFINED) {
    // Removes ShelfItem for |window| when it has a ShelfItem.
    if (window->GetIntProperty(WmWindowProperty::SHELF_ID) != kInvalidShelfID)
      RemoveShelfItem(window);
    return;
  }

  // Update an existing ShelfItem for |window| when a property has changed.
  if (HasShelfItemForWindow(window)) {
    int index = GetShelfItemIndexForWindow(window);
    DCHECK_GE(index, 0);
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
  if (gained_active && HasShelfItemForWindow(gained_active))
    UpdateShelfItemStatus(gained_active, true);
  if (lost_active && HasShelfItemForWindow(lost_active))
    UpdateShelfItemStatus(lost_active, false);
}

void ShelfWindowWatcher::OnDisplayAdded(const display::Display& new_display) {
  WmWindow* root = WmShell::Get()->GetRootWindowForDisplayId(new_display.id());

  // When the primary root window's display get removed, the existing root
  // window is taken over by the new display and the observer is already set.
  WmWindow* container =
      root->GetChildByShellWindowId(kShellWindowId_DefaultContainer);
  if (!observed_container_windows_.IsObserving(container))
    observed_container_windows_.Add(container);
}

void ShelfWindowWatcher::OnDisplayRemoved(const display::Display& old_display) {
}

void ShelfWindowWatcher::OnDisplayMetricsChanged(const display::Display&,
                                                 uint32_t) {}

}  // namespace ash
