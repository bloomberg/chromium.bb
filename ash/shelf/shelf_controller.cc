// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_controller.h"

#include "ash/public/interfaces/shelf.mojom.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/app_list_shelf_item_delegate.h"
#include "ash/shelf/wm_shelf.h"
#include "ash/shell.h"
#include "ash/wm_window.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ash {

namespace {

// Returns the WmShelf instance for the display with the given |display_id|.
WmShelf* GetShelfForDisplay(int64_t display_id) {
  // The controller may be null for invalid ids or for displays being removed.
  RootWindowController* root_window_controller =
      Shell::GetRootWindowControllerWithDisplayId(display_id);
  return root_window_controller ? root_window_controller->GetShelf() : nullptr;
}

}  // namespace

ShelfController::ShelfController() {
  // Create the app list item in the shelf.
  AppListShelfItemDelegate::CreateAppListItemAndDelegate(&model_);
}

ShelfController::~ShelfController() {}

void ShelfController::BindRequest(mojom::ShelfControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void ShelfController::NotifyShelfCreated(WmShelf* shelf) {
  // Notify observers, Chrome will set alignment and auto-hide from prefs.
  int64_t display_id = shelf->GetWindow()->GetDisplayNearestWindow().id();
  observers_.ForAllPtrs([display_id](mojom::ShelfObserver* observer) {
    observer->OnShelfCreated(display_id);
  });
}

void ShelfController::NotifyShelfAlignmentChanged(WmShelf* shelf) {
  ShelfAlignment alignment = shelf->alignment();
  int64_t display_id = shelf->GetWindow()->GetDisplayNearestWindow().id();
  observers_.ForAllPtrs(
      [alignment, display_id](mojom::ShelfObserver* observer) {
        observer->OnAlignmentChanged(alignment, display_id);
      });
}

void ShelfController::NotifyShelfAutoHideBehaviorChanged(WmShelf* shelf) {
  ShelfAutoHideBehavior behavior = shelf->auto_hide_behavior();
  int64_t display_id = shelf->GetWindow()->GetDisplayNearestWindow().id();
  observers_.ForAllPtrs([behavior, display_id](mojom::ShelfObserver* observer) {
    observer->OnAutoHideBehaviorChanged(behavior, display_id);
  });
}

void ShelfController::AddObserver(
    mojom::ShelfObserverAssociatedPtrInfo observer) {
  mojom::ShelfObserverAssociatedPtr observer_ptr;
  observer_ptr.Bind(std::move(observer));
  observers_.AddPtr(std::move(observer_ptr));
}

void ShelfController::SetAlignment(ShelfAlignment alignment,
                                   int64_t display_id) {
  WmShelf* shelf = GetShelfForDisplay(display_id);
  // TODO(jamescook): The initialization check should not be necessary, but
  // otherwise this wrongly tries to set the alignment on a secondary display
  // during login before the ShelfLockingManager and ShelfView are created.
  if (shelf && shelf->IsShelfInitialized())
    shelf->SetAlignment(alignment);
}

void ShelfController::SetAutoHideBehavior(ShelfAutoHideBehavior auto_hide,
                                          int64_t display_id) {
  WmShelf* shelf = GetShelfForDisplay(display_id);
  // TODO(jamescook): The initialization check should not be necessary, but
  // otherwise this wrongly tries to set auto-hide state on a secondary display
  // during login before the ShelfView is created.
  if (shelf && shelf->IsShelfInitialized())
    shelf->SetAutoHideBehavior(auto_hide);
}

void ShelfController::PinItem(
    const ShelfItem& item,
    mojom::ShelfItemDelegateAssociatedPtrInfo delegate) {
  NOTIMPLEMENTED();
}

void ShelfController::UnpinItem(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void ShelfController::SetItemImage(const std::string& app_id,
                                   const SkBitmap& image) {
  NOTIMPLEMENTED();
}

}  // namespace ash
