// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/shelf.h"
#include "ash/common/shelf/shelf_delegate.h"
#include "ash/common/shelf/shelf_item_delegate.h"
#include "ash/common/shelf/shelf_layout_manager.h"
#include "ash/common/shelf/shelf_locking_manager.h"
#include "ash/common/shelf/shelf_model.h"
#include "ash/common/shelf/shelf_widget.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/shelf/wm_shelf_observer.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "base/logging.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

// static
WmShelf* WmShelf::ForWindow(WmWindow* window) {
  return window->GetRootWindowController()->GetShelf();
}

void WmShelf::SetShelf(Shelf* shelf) {
  DCHECK(!shelf_);
  DCHECK(shelf);
  shelf_ = shelf;
  DCHECK(shelf_layout_manager_);
  shelf_locking_manager_.reset(new ShelfLockingManager(this));
  // When the shelf is created the alignment is unlocked. Chrome will update the
  // alignment later from preferences.
  alignment_ = SHELF_ALIGNMENT_BOTTOM;
}

void WmShelf::ClearShelf() {
  DCHECK(shelf_);
  shelf_locking_manager_.reset();
  shelf_ = nullptr;
}

void WmShelf::SetShelfLayoutManager(ShelfLayoutManager* manager) {
  DCHECK(!shelf_layout_manager_);
  DCHECK(manager);
  shelf_layout_manager_ = manager;
  shelf_layout_manager_->AddObserver(this);
}

WmWindow* WmShelf::GetWindow() {
  // Use |shelf_layout_manager_| to access ShelfWidget because it is set before
  // before the Shelf instance is available.
  return WmLookup::Get()->GetWindowForWidget(
      shelf_layout_manager_->shelf_widget());
}

void WmShelf::SetAlignment(ShelfAlignment alignment) {
  DCHECK(shelf_layout_manager_);

  if (alignment_ == alignment)
    return;

  if (shelf_locking_manager_->is_locked() &&
      alignment != SHELF_ALIGNMENT_BOTTOM_LOCKED) {
    shelf_locking_manager_->set_stored_alignment(alignment);
    return;
  }

  alignment_ = alignment;
  // The ShelfWidget notifies the ShelfView of the alignment change.
  shelf_layout_manager_->shelf_widget()->OnShelfAlignmentChanged();
  WmShell::Get()->shelf_delegate()->OnShelfAlignmentChanged(this);
  WmShell::Get()->NotifyShelfAlignmentChanged(GetWindow()->GetRootWindow());
  // ShelfLayoutManager will resize the shelf.
}

bool WmShelf::IsHorizontalAlignment() const {
  switch (alignment_) {
    case SHELF_ALIGNMENT_BOTTOM:
    case SHELF_ALIGNMENT_BOTTOM_LOCKED:
      return true;
    case SHELF_ALIGNMENT_LEFT:
    case SHELF_ALIGNMENT_RIGHT:
      return false;
  }
  NOTREACHED();
  return true;
}

int WmShelf::SelectValueForShelfAlignment(int bottom,
                                          int left,
                                          int right) const {
  switch (alignment_) {
    case SHELF_ALIGNMENT_BOTTOM:
    case SHELF_ALIGNMENT_BOTTOM_LOCKED:
      return bottom;
    case SHELF_ALIGNMENT_LEFT:
      return left;
    case SHELF_ALIGNMENT_RIGHT:
      return right;
  }
  NOTREACHED();
  return bottom;
}

int WmShelf::PrimaryAxisValue(int horizontal, int vertical) const {
  return IsHorizontalAlignment() ? horizontal : vertical;
}

void WmShelf::SetAutoHideBehavior(ShelfAutoHideBehavior auto_hide_behavior) {
  DCHECK(shelf_layout_manager_);

  if (auto_hide_behavior_ == auto_hide_behavior)
    return;

  auto_hide_behavior_ = auto_hide_behavior;
  WmShell::Get()->shelf_delegate()->OnShelfAutoHideBehaviorChanged(this);
  WmShell::Get()->NotifyShelfAutoHideBehaviorChanged(
      GetWindow()->GetRootWindow());
}

ShelfAutoHideState WmShelf::GetAutoHideState() const {
  return shelf_layout_manager_->auto_hide_state();
}

void WmShelf::UpdateAutoHideState() {
  shelf_layout_manager_->UpdateAutoHideState();
}

ShelfBackgroundType WmShelf::GetBackgroundType() const {
  return shelf_layout_manager_->shelf_widget()->GetBackgroundType();
}

WmDimmerView* WmShelf::CreateDimmerView(bool disable_animations_for_test) {
  return nullptr;
}

bool WmShelf::IsDimmed() const {
  return shelf_layout_manager_->shelf_widget()->GetDimsShelf();
}

bool WmShelf::IsVisible() const {
  return shelf_->shelf_widget()->IsShelfVisible();
}

void WmShelf::UpdateVisibilityState() {
  if (shelf_layout_manager_)
    shelf_layout_manager_->UpdateVisibilityState();
}

ShelfVisibilityState WmShelf::GetVisibilityState() const {
  return shelf_layout_manager_ ? shelf_layout_manager_->visibility_state()
                               : SHELF_HIDDEN;
}

gfx::Rect WmShelf::GetIdealBounds() {
  return shelf_layout_manager_->GetIdealBounds();
}

gfx::Rect WmShelf::GetUserWorkAreaBounds() const {
  return shelf_layout_manager_ ? shelf_layout_manager_->user_work_area_bounds()
                               : gfx::Rect();
}

void WmShelf::UpdateIconPositionForPanel(WmWindow* panel) {
  shelf_layout_manager_->shelf_widget()->UpdateIconPositionForPanel(panel);
}

gfx::Rect WmShelf::GetScreenBoundsOfItemIconForWindow(WmWindow* window) {
  if (!shelf_layout_manager_)
    return gfx::Rect();
  return shelf_layout_manager_->shelf_widget()
      ->GetScreenBoundsOfItemIconForWindow(window);
}

// static
void WmShelf::LaunchShelfItem(int item_index) {
  ShelfModel* shelf_model = WmShell::Get()->shelf_model();
  const ShelfItems& items = shelf_model->items();
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
    ActivateShelfItem(found_index);
  }
}

// static
void WmShelf::ActivateShelfItem(int item_index) {
  // We pass in a keyboard event which will then trigger a switch to the
  // next item if the current one is already active.
  ui::KeyEvent event(ui::ET_KEY_RELEASED,
                     ui::VKEY_UNKNOWN,  // The actual key gets ignored.
                     ui::EF_NONE);

  ShelfModel* shelf_model = WmShell::Get()->shelf_model();
  const ShelfItem& item = shelf_model->items()[item_index];
  ShelfItemDelegate* item_delegate = shelf_model->GetShelfItemDelegate(item.id);
  item_delegate->ItemSelected(event);
}

bool WmShelf::ProcessGestureEvent(const ui::GestureEvent& event) {
  // Can be called at login screen.
  if (!shelf_layout_manager_)
    return false;
  return shelf_layout_manager_->ProcessGestureEvent(event);
}

void WmShelf::AddObserver(WmShelfObserver* observer) {
  observers_.AddObserver(observer);
}

void WmShelf::RemoveObserver(WmShelfObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WmShelf::NotifyShelfIconPositionsChanged() {
  FOR_EACH_OBSERVER(WmShelfObserver, observers_, OnShelfIconPositionsChanged());
}

StatusAreaWidget* WmShelf::GetStatusAreaWidget() const {
  return shelf_layout_manager_->shelf_widget()->status_area_widget();
}

void WmShelf::SetVirtualKeyboardBoundsForTesting(const gfx::Rect& bounds) {
  shelf_layout_manager_->OnKeyboardBoundsChanging(bounds);
}

ShelfLockingManager* WmShelf::GetShelfLockingManagerForTesting() {
  return shelf_locking_manager_.get();
}

ShelfView* WmShelf::GetShelfViewForTesting() {
  return shelf_->shelf_view_for_testing();
}

ShelfWidget* WmShelf::GetShelfWidgetForTesting() {
  return shelf_layout_manager_->shelf_widget();
}

WmShelf::WmShelf() {}

WmShelf::~WmShelf() {}

void WmShelf::WillDeleteShelfLayoutManager() {
  DCHECK(shelf_layout_manager_);
  shelf_layout_manager_->RemoveObserver(this);
  shelf_layout_manager_ = nullptr;
}

void WmShelf::WillChangeVisibilityState(ShelfVisibilityState new_state) {
  FOR_EACH_OBSERVER(WmShelfObserver, observers_,
                    WillChangeVisibilityState(new_state));
}

void WmShelf::OnAutoHideStateChanged(ShelfAutoHideState new_state) {
  FOR_EACH_OBSERVER(WmShelfObserver, observers_,
                    OnAutoHideStateChanged(new_state));
}

void WmShelf::OnBackgroundUpdated(ShelfBackgroundType background_type,
                                  BackgroundAnimatorChangeType change_type) {
  if (background_type == GetBackgroundType())
    return;
  FOR_EACH_OBSERVER(WmShelfObserver, observers_,
                    OnBackgroundTypeChanged(background_type, change_type));
}

}  // namespace ash
