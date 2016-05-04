// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/aura/wm_shelf_aura.h"

#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/wm/aura/wm_window_aura.h"
#include "ash/wm/common/shelf/wm_shelf_observer.h"
#include "ash/wm/common/wm_window.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace wm {

WmShelfAura::WmShelfAura(Shelf* shelf)
    : shelf_(shelf), shelf_layout_manager_(shelf->shelf_layout_manager()) {
  shelf_layout_manager_->AddObserver(this);
  shelf_->AddIconObserver(this);
}

WmShelfAura::~WmShelfAura() {
  shelf_->RemoveIconObserver(this);
  if (shelf_layout_manager_)
    shelf_layout_manager_->RemoveObserver(this);
}

// static
Shelf* WmShelfAura::GetShelf(WmShelf* shelf) {
  return static_cast<WmShelfAura*>(shelf)->shelf_;
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

gfx::Rect WmShelfAura::GetScreenBoundsOfItemIconForWindow(
    wm::WmWindow* window) {
  return shelf_->GetScreenBoundsOfItemIconForWindow(
      WmWindowAura::GetAuraWindow(window));
}

void WmShelfAura::AddObserver(WmShelfObserver* observer) {
  observers_.AddObserver(observer);
}

void WmShelfAura::RemoveObserver(WmShelfObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WmShelfAura::WillDeleteShelf() {
  shelf_layout_manager_->RemoveObserver(this);
  shelf_layout_manager_ = nullptr;
}

void WmShelfAura::OnBackgroundUpdated(
    wm::ShelfBackgroundType background_type,
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

}  // namespace wm
}  // namespace ash
