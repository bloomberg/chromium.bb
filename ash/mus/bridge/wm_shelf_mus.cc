// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/bridge/wm_shelf_mus.h"

#include "ash/common/shell_window_ids.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/mus/bridge/wm_window_mus.h"
#include "services/ui/public/cpp/window.h"
#include "ui/views/widget/widget.h"

// TODO(sky): fully implement this http://crbug.com/612631 .
#undef NOTIMPLEMENTED
#define NOTIMPLEMENTED() DVLOG(1) << "notimplemented"

namespace ash {
namespace mus {

WmShelfMus::WmShelfMus(WmRootWindowController* root_window_controller) {
  DCHECK(root_window_controller);
  // Create a placeholder shelf widget, because the status area code assumes it
  // can access one.
  // TODO(jamescook): Create a real shelf widget. http://crbug.com/615155
  shelf_widget_ = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  root_window_controller->ConfigureWidgetInitParamsForContainer(
      shelf_widget_, kShellWindowId_ShelfContainer, &params);
  shelf_widget_->Init(params);
}

WmShelfMus::~WmShelfMus() {}

WmWindow* WmShelfMus::GetWindow() {
  return WmWindowMus::Get(shelf_widget_);
}

ShelfAlignment WmShelfMus::GetAlignment() const {
  NOTIMPLEMENTED();
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

WmDimmerView* WmShelfMus::CreateDimmerView(bool disable_animations_for_test) {
  // mus does not dim shelf items.
  return nullptr;
}

bool WmShelfMus::IsDimmed() const {
  // mus does not dim shelf items.
  return false;
}

void WmShelfMus::SchedulePaint() {
  NOTIMPLEMENTED();
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
  return SHELF_VISIBLE;
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

bool WmShelfMus::ProcessGestureEvent(const ui::GestureEvent& event) {
  NOTIMPLEMENTED();
  return false;
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

ShelfLockingManager* WmShelfMus::GetShelfLockingManagerForTesting() {
  NOTIMPLEMENTED();
  return nullptr;
}

ShelfView* WmShelfMus::GetShelfViewForTesting() {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace mus
}  // namespace ash
