// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/bridge/wm_shelf_mus.h"

#include "ash/mus/bridge/wm_window_mus.h"
#include "ash/mus/shelf_layout_manager.h"
#include "components/mus/public/cpp/window.h"

// TODO(sky): fully implement this http://crbug.com/612631 .
#undef NOTIMPLEMENTED
#define NOTIMPLEMENTED() DVLOG(1) << "notimplemented"

namespace ash {
namespace mus {

WmShelfMus::WmShelfMus(ShelfLayoutManager* shelf_layout_manager)
    : shelf_layout_manager_(shelf_layout_manager) {}

WmShelfMus::~WmShelfMus() {}

WmWindow* WmShelfMus::GetWindow() {
  return WmWindowMus::Get(shelf_layout_manager_->GetShelfWindow());
}

ShelfAlignment WmShelfMus::GetAlignment() const {
  switch (shelf_layout_manager_->alignment()) {
    case mash::shelf::mojom::Alignment::BOTTOM:
      return SHELF_ALIGNMENT_BOTTOM;
    case mash::shelf::mojom::Alignment::LEFT:
      return SHELF_ALIGNMENT_LEFT;
    case mash::shelf::mojom::Alignment::RIGHT:
      return SHELF_ALIGNMENT_RIGHT;
  }
  NOTREACHED();
  return SHELF_ALIGNMENT_BOTTOM;
}

void WmShelfMus::SetAlignment(ShelfAlignment alignment) {
  NOTIMPLEMENTED();
}

ShelfAutoHideBehavior WmShelfMus::GetAutoHideBehavior() const {
  NOTIMPLEMENTED();
  return SHELF_AUTO_HIDE_BEHAVIOR_NEVER;
}

void WmShelfMus::SetAutoHideBehavior(ShelfAutoHideBehavior behavior) {
  NOTIMPLEMENTED();
}

ShelfAutoHideState WmShelfMus::GetAutoHideState() const {
  NOTIMPLEMENTED();
  return SHELF_AUTO_HIDE_HIDDEN;
}

void WmShelfMus::UpdateAutoHideState() {
  NOTIMPLEMENTED();
}

ShelfBackgroundType WmShelfMus::GetBackgroundType() const {
  NOTIMPLEMENTED();
  return SHELF_BACKGROUND_DEFAULT;
}

bool WmShelfMus::IsDimmed() const {
  NOTIMPLEMENTED();
  return false;
}

bool WmShelfMus::IsVisible() const {
  NOTIMPLEMENTED();
  return true;
}

void WmShelfMus::UpdateVisibilityState() {
  NOTIMPLEMENTED();
}

ShelfVisibilityState WmShelfMus::GetVisibilityState() const {
  NOTIMPLEMENTED();
  return shelf_layout_manager_->GetShelfWindow() ? SHELF_VISIBLE : SHELF_HIDDEN;
}

gfx::Rect WmShelfMus::GetUserWorkAreaBounds() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

void WmShelfMus::UpdateIconPositionForWindow(WmWindow* window) {
  NOTIMPLEMENTED();
}

gfx::Rect WmShelfMus::GetIdealBounds() {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

gfx::Rect WmShelfMus::GetScreenBoundsOfItemIconForWindow(WmWindow* window) {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

bool WmShelfMus::ProcessGestureEvent(const ui::GestureEvent& event,
                                     WmWindow* target_window) {
  NOTIMPLEMENTED();
  return false;
}

void WmShelfMus::UpdateAutoHideForMouseEvent(ui::MouseEvent* event) {
  NOTIMPLEMENTED();
}

void WmShelfMus::UpdateAutoHideForGestureEvent(ui::GestureEvent* event) {
  NOTIMPLEMENTED();
}

void WmShelfMus::AddObserver(WmShelfObserver* observer) {
  observers_.AddObserver(observer);
}

void WmShelfMus::RemoveObserver(WmShelfObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WmShelfMus::SetKeyboardBoundsForTesting(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

}  // namespace mus
}  // namespace ash
