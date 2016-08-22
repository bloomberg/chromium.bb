// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/shelf/wm_shelf_observer.h"
#include "ash/common/wm_lookup.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "base/logging.h"

namespace ash {

void WmShelf::SetShelf(Shelf* shelf) {
  DCHECK(!shelf_);
  DCHECK(shelf);
  shelf_ = shelf;
}

void WmShelf::ClearShelf() {
  DCHECK(shelf_);
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

ShelfAlignment WmShelf::GetAlignment() const {
  return shelf_ ? shelf_->alignment() : SHELF_ALIGNMENT_BOTTOM_LOCKED;
}

void WmShelf::SetAlignment(ShelfAlignment alignment) {
  shelf_->SetAlignment(alignment);
}

bool WmShelf::IsHorizontalAlignment() const {
  switch (GetAlignment()) {
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
  switch (GetAlignment()) {
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

ShelfAutoHideBehavior WmShelf::GetAutoHideBehavior() const {
  return shelf_->auto_hide_behavior();
}

void WmShelf::SetAutoHideBehavior(ShelfAutoHideBehavior behavior) {
  shelf_->SetAutoHideBehavior(behavior);
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
  return shelf_->IsVisible();
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

void WmShelf::UpdateIconPositionForWindow(WmWindow* window) {
  shelf_->UpdateIconPositionForWindow(window);
}

gfx::Rect WmShelf::GetScreenBoundsOfItemIconForWindow(WmWindow* window) {
  return shelf_->GetScreenBoundsOfItemIconForWindow(window);
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
  return shelf_->shelf_locking_manager_for_testing();
}

ShelfView* WmShelf::GetShelfViewForTesting() {
  return shelf_->shelf_view_for_testing();
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
