// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/aura/wm_shelf_aura.h"

#include "ash/aura/wm_window_aura.h"
#include "ash/common/shelf/wm_shelf_observer.h"
#include "ash/common/wm_window.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ui/views/widget/widget.h"

namespace ash {

WmShelfAura::WmShelfAura() {}

WmShelfAura::~WmShelfAura() {}

void WmShelfAura::SetShelfLayoutManager(
    ShelfLayoutManager* shelf_layout_manager) {
  DCHECK(!shelf_layout_manager_);
  shelf_layout_manager_ = shelf_layout_manager;
  shelf_layout_manager_->AddObserver(this);
}

void WmShelfAura::SetShelf(Shelf* shelf) {
  DCHECK(!shelf_);
  shelf_ = shelf;
  shelf_->AddIconObserver(this);
}

void WmShelfAura::Shutdown() {
  if (shelf_) {
    shelf_->RemoveIconObserver(this);
    shelf_ = nullptr;
  }

  ResetShelfLayoutManager();
}

// static
Shelf* WmShelfAura::GetShelf(WmShelf* shelf) {
  return static_cast<WmShelfAura*>(shelf)->shelf_;
}

void WmShelfAura::ResetShelfLayoutManager() {
  if (!shelf_layout_manager_)
    return;
  shelf_layout_manager_->RemoveObserver(this);
  shelf_layout_manager_ = nullptr;
}

WmWindow* WmShelfAura::GetWindow() {
  return WmWindowAura::Get(shelf_->shelf_widget()->GetNativeView());
}

ShelfAlignment WmShelfAura::GetAlignment() const {
  return shelf_->alignment();
}

ShelfBackgroundType WmShelfAura::GetBackgroundType() const {
  return shelf_->shelf_widget()->GetBackgroundType();
}

void WmShelfAura::UpdateVisibilityState() {
  shelf_->shelf_layout_manager()->UpdateVisibilityState();
}

ShelfVisibilityState WmShelfAura::GetVisibilityState() const {
  return shelf_layout_manager_ ? shelf_layout_manager_->visibility_state()
                               : SHELF_HIDDEN;
}

void WmShelfAura::UpdateIconPositionForWindow(WmWindow* window) {
  shelf_->UpdateIconPositionForWindow(WmWindowAura::GetAuraWindow(window));
}

gfx::Rect WmShelfAura::GetScreenBoundsOfItemIconForWindow(WmWindow* window) {
  return shelf_->GetScreenBoundsOfItemIconForWindow(
      WmWindowAura::GetAuraWindow(window));
}

void WmShelfAura::AddObserver(WmShelfObserver* observer) {
  observers_.AddObserver(observer);
}

void WmShelfAura::RemoveObserver(WmShelfObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WmShelfAura::WillDeleteShelfLayoutManager() {
  ResetShelfLayoutManager();
}

void WmShelfAura::OnBackgroundUpdated(
    ShelfBackgroundType background_type,
    BackgroundAnimatorChangeType change_type) {
  FOR_EACH_OBSERVER(WmShelfObserver, observers_,
                    OnBackgroundUpdated(background_type, change_type));
}

void WmShelfAura::WillChangeVisibilityState(ShelfVisibilityState new_state) {
  FOR_EACH_OBSERVER(WmShelfObserver, observers_,
                    WillChangeVisibilityState(new_state));
}

void WmShelfAura::OnShelfIconPositionsChanged() {
  FOR_EACH_OBSERVER(WmShelfObserver, observers_, OnShelfIconPositionsChanged());
}

}  // namespace ash
