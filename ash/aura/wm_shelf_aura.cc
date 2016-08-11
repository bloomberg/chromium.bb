// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/aura/wm_shelf_aura.h"

#include "ash/aura/wm_window_aura.h"
#include "ash/common/shelf/wm_shelf_observer.h"
#include "ash/common/wm_window.h"
#include "ash/shelf/dimmer_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_bezel_event_handler.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ui/views/widget/widget.h"

namespace ash {

// WmShelfAura::AutoHideEventHandler -------------------------------------------

// Forwards mouse and gesture events to ShelfLayoutManager for auto-hide.
// TODO(mash): Add similar event handling support for mash.
class WmShelfAura::AutoHideEventHandler : public ui::EventHandler {
 public:
  explicit AutoHideEventHandler(ShelfLayoutManager* shelf_layout_manager)
      : shelf_layout_manager_(shelf_layout_manager) {
    Shell::GetInstance()->AddPreTargetHandler(this);
  }
  ~AutoHideEventHandler() override {
    Shell::GetInstance()->RemovePreTargetHandler(this);
  }

  // Overridden from ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    shelf_layout_manager_->UpdateAutoHideForMouseEvent(
        event, WmWindowAura::Get(static_cast<aura::Window*>(event->target())));
  }
  void OnGestureEvent(ui::GestureEvent* event) override {
    shelf_layout_manager_->UpdateAutoHideForGestureEvent(
        event, WmWindowAura::Get(static_cast<aura::Window*>(event->target())));
  }

 private:
  ShelfLayoutManager* shelf_layout_manager_;
  DISALLOW_COPY_AND_ASSIGN(AutoHideEventHandler);
};

// WmShelfAura -----------------------------------------------------------------

WmShelfAura::WmShelfAura() {}

WmShelfAura::~WmShelfAura() {}

void WmShelfAura::SetShelfLayoutManager(
    ShelfLayoutManager* shelf_layout_manager) {
  DCHECK(!shelf_layout_manager_);
  shelf_layout_manager_ = shelf_layout_manager;
  shelf_layout_manager_->AddObserver(this);
  bezel_event_handler_.reset(new ShelfBezelEventHandler(this));
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
  // Clear event handlers that might otherwise forward events during shutdown.
  auto_hide_event_handler_.reset();
  bezel_event_handler_.reset();
  shelf_layout_manager_->RemoveObserver(this);
  shelf_layout_manager_ = nullptr;
}

WmWindow* WmShelfAura::GetWindow() {
  // Use |shelf_layout_manager_| to access ShelfWidget because it is set
  // before |shelf_| is available.
  return WmWindowAura::Get(
      shelf_layout_manager_->shelf_widget()->GetNativeView());
}

ShelfAlignment WmShelfAura::GetAlignment() const {
  // Can be called before |shelf_| is set when initializing a secondary monitor.
  return shelf_ ? shelf_->alignment() : SHELF_ALIGNMENT_BOTTOM_LOCKED;
}

void WmShelfAura::SetAlignment(ShelfAlignment alignment) {
  shelf_->SetAlignment(alignment);
}

ShelfAutoHideBehavior WmShelfAura::GetAutoHideBehavior() const {
  return shelf_->auto_hide_behavior();
}

void WmShelfAura::SetAutoHideBehavior(ShelfAutoHideBehavior behavior) {
  shelf_->SetAutoHideBehavior(behavior);
}

ShelfAutoHideState WmShelfAura::GetAutoHideState() const {
  return shelf_layout_manager_->auto_hide_state();
}

void WmShelfAura::UpdateAutoHideState() {
  shelf_layout_manager_->UpdateAutoHideState();
}

ShelfBackgroundType WmShelfAura::GetBackgroundType() const {
  return shelf_layout_manager_->shelf_widget()->GetBackgroundType();
}

WmDimmerView* WmShelfAura::CreateDimmerView(bool disable_animations_for_test) {
  return DimmerView::Create(this, disable_animations_for_test);
}

bool WmShelfAura::IsDimmed() const {
  return shelf_layout_manager_->shelf_widget()->GetDimsShelf();
}

bool WmShelfAura::IsShowingOverflowBubble() const {
  return shelf_->IsShowingOverflowBubble();
}

void WmShelfAura::SchedulePaint() {
  // Can be called during shutdown if the overflow bubble is visible.
  if (shelf_)
    shelf_->SchedulePaint();
}

bool WmShelfAura::IsVisible() const {
  return shelf_->IsVisible();
}

void WmShelfAura::UpdateVisibilityState() {
  if (shelf_layout_manager_)
    shelf_layout_manager_->UpdateVisibilityState();
}

ShelfVisibilityState WmShelfAura::GetVisibilityState() const {
  return shelf_layout_manager_ ? shelf_layout_manager_->visibility_state()
                               : SHELF_HIDDEN;
}

gfx::Rect WmShelfAura::GetIdealBounds() {
  return shelf_layout_manager_->GetIdealBounds();
}

gfx::Rect WmShelfAura::GetUserWorkAreaBounds() const {
  return shelf_layout_manager_ ? shelf_layout_manager_->user_work_area_bounds()
                               : gfx::Rect();
}

void WmShelfAura::UpdateIconPositionForWindow(WmWindow* window) {
  shelf_->UpdateIconPositionForWindow(WmWindowAura::GetAuraWindow(window));
}

gfx::Rect WmShelfAura::GetScreenBoundsOfItemIconForWindow(WmWindow* window) {
  return shelf_->GetScreenBoundsOfItemIconForWindow(
      WmWindowAura::GetAuraWindow(window));
}

bool WmShelfAura::ProcessGestureEvent(const ui::GestureEvent& event) {
  // Can be called at login screen.
  if (!shelf_layout_manager_)
    return false;
  return shelf_layout_manager_->ProcessGestureEvent(event);
}

void WmShelfAura::AddObserver(WmShelfObserver* observer) {
  observers_.AddObserver(observer);
}

void WmShelfAura::RemoveObserver(WmShelfObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WmShelfAura::SetKeyboardBoundsForTesting(const gfx::Rect& bounds) {
  shelf_layout_manager_->OnKeyboardBoundsChanging(bounds);
}

ShelfLockingManager* WmShelfAura::GetShelfLockingManagerForTesting() {
  return shelf_->shelf_locking_manager_for_testing();
}

ShelfView* WmShelfAura::GetShelfViewForTesting() {
  return shelf_->shelf_view_for_testing();
}

void WmShelfAura::WillDeleteShelfLayoutManager() {
  ResetShelfLayoutManager();
}

void WmShelfAura::OnBackgroundUpdated(
    ShelfBackgroundType background_type,
    BackgroundAnimatorChangeType change_type) {
  if (background_type == GetBackgroundType())
    return;
  FOR_EACH_OBSERVER(WmShelfObserver, observers_,
                    OnBackgroundTypeChanged(background_type, change_type));
}

void WmShelfAura::WillChangeVisibilityState(ShelfVisibilityState new_state) {
  FOR_EACH_OBSERVER(WmShelfObserver, observers_,
                    WillChangeVisibilityState(new_state));

  if (new_state != SHELF_AUTO_HIDE) {
    auto_hide_event_handler_.reset();
  } else if (!auto_hide_event_handler_) {
    auto_hide_event_handler_.reset(
        new AutoHideEventHandler(shelf_layout_manager_));
  }
}

void WmShelfAura::OnAutoHideStateChanged(ShelfAutoHideState new_state) {
  FOR_EACH_OBSERVER(WmShelfObserver, observers_,
                    OnAutoHideStateChanged(new_state));
}

void WmShelfAura::OnShelfIconPositionsChanged() {
  FOR_EACH_OBSERVER(WmShelfObserver, observers_, OnShelfIconPositionsChanged());
}

}  // namespace ash
