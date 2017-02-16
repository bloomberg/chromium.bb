// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/wm_shelf.h"

#include "ash/common/shelf/shelf_controller.h"
#include "ash/common/shelf/shelf_delegate.h"
#include "ash/common/shelf/shelf_item_delegate.h"
#include "ash/common/shelf/shelf_layout_manager.h"
#include "ash/common/shelf/shelf_locking_manager.h"
#include "ash/common/shelf/shelf_model.h"
#include "ash/common/shelf/shelf_widget.h"
#include "ash/common/shelf/wm_shelf_observer.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_bezel_event_handler.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "ui/aura/env.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

// WmShelf::AutoHideEventHandler -----------------------------------------------

// Forwards mouse and gesture events to ShelfLayoutManager for auto-hide.
// TODO(mash): Add similar event handling support for mash.
class WmShelf::AutoHideEventHandler : public ui::EventHandler {
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
        event, WmWindow::Get(static_cast<aura::Window*>(event->target())));
  }
  void OnGestureEvent(ui::GestureEvent* event) override {
    shelf_layout_manager_->UpdateAutoHideForGestureEvent(
        event, WmWindow::Get(static_cast<aura::Window*>(event->target())));
  }

 private:
  ShelfLayoutManager* shelf_layout_manager_;
  DISALLOW_COPY_AND_ASSIGN(AutoHideEventHandler);
};

// WmShelf ---------------------------------------------------------------------

WmShelf::WmShelf() {}

WmShelf::~WmShelf() {}

// static
WmShelf* WmShelf::ForWindow(WmWindow* window) {
  return window->GetRootWindowController()->GetShelf();
}

// static
bool WmShelf::CanChangeShelfAlignment() {
  if (WmShell::Get()->system_tray_delegate()->IsUserSupervised())
    return false;

  LoginStatus login_status =
      WmShell::Get()->system_tray_delegate()->GetUserLoginStatus();

  switch (login_status) {
    case LoginStatus::LOCKED:
    // Shelf alignment changes can be requested while being locked, but will
    // be applied upon unlock.
    case LoginStatus::USER:
    case LoginStatus::OWNER:
      return true;
    case LoginStatus::PUBLIC:
    case LoginStatus::SUPERVISED:
    case LoginStatus::GUEST:
    case LoginStatus::KIOSK_APP:
    case LoginStatus::ARC_KIOSK_APP:
    case LoginStatus::NOT_LOGGED_IN:
      return false;
  }

  NOTREACHED();
  return false;
}

void WmShelf::CreateShelfWidget(WmWindow* root) {
  DCHECK(!shelf_widget_);
  WmWindow* shelf_container =
      root->GetChildByShellWindowId(kShellWindowId_ShelfContainer);
  shelf_widget_.reset(new ShelfWidget(shelf_container, this));

  DCHECK(!shelf_layout_manager_);
  shelf_layout_manager_ = shelf_widget_->shelf_layout_manager();
  shelf_layout_manager_->AddObserver(this);

  // Must occur after |shelf_widget_| is constructed because the system tray
  // constructors call back into WmShelf::shelf_widget().
  DCHECK(!shelf_widget_->status_area_widget());
  WmWindow* status_container =
      root->GetChildByShellWindowId(kShellWindowId_StatusContainer);
  shelf_widget_->CreateStatusAreaWidget(status_container);

  // TODO: ShelfBezelEventHandler needs to work with mus too.
  // http://crbug.com/636647
  if (aura::Env::GetInstance()->mode() == aura::Env::Mode::LOCAL)
    bezel_event_handler_ = base::MakeUnique<ShelfBezelEventHandler>(this);
}

void WmShelf::ShutdownShelfWidget() {
  if (shelf_widget_)
    shelf_widget_->Shutdown();
}

void WmShelf::DestroyShelfWidget() {
  shelf_widget_.reset();
}

void WmShelf::CreateShelfView() {
  DCHECK(shelf_layout_manager_);
  DCHECK(shelf_widget_);
  DCHECK(!shelf_view_);
  shelf_view_ = shelf_widget_->CreateShelfView();
  shelf_locking_manager_.reset(new ShelfLockingManager(this));
  WmShell::Get()->shelf_controller()->NotifyShelfCreated(this);
}

void WmShelf::ShutdownShelf() {
  DCHECK(shelf_view_);
  shelf_locking_manager_.reset();
  shelf_view_ = nullptr;
}

bool WmShelf::IsShelfInitialized() const {
  return !!shelf_view_;
}

WmWindow* WmShelf::GetWindow() {
  return WmLookup::Get()->GetWindowForWidget(shelf_widget_.get());
}

void WmShelf::SetAlignment(ShelfAlignment alignment) {
  DCHECK(shelf_layout_manager_);
  DCHECK(shelf_locking_manager_);

  if (alignment_ == alignment)
    return;

  if (shelf_locking_manager_->is_locked() &&
      alignment != SHELF_ALIGNMENT_BOTTOM_LOCKED) {
    shelf_locking_manager_->set_stored_alignment(alignment);
    return;
  }

  alignment_ = alignment;
  // The ShelfWidget notifies the ShelfView of the alignment change.
  shelf_widget_->OnShelfAlignmentChanged();
  shelf_layout_manager_->LayoutShelf();
  WmShell::Get()->shelf_controller()->NotifyShelfAlignmentChanged(this);
  WmShell::Get()->NotifyShelfAlignmentChanged(GetWindow()->GetRootWindow());
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
  WmShell::Get()->shelf_controller()->NotifyShelfAutoHideBehaviorChanged(this);
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
  return shelf_widget_->GetBackgroundType();
}

bool WmShelf::IsVisible() const {
  return shelf_widget_->IsShelfVisible();
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
  shelf_widget_->UpdateIconPositionForPanel(panel);
}

gfx::Rect WmShelf::GetScreenBoundsOfItemIconForWindow(WmWindow* window) {
  if (!shelf_widget_)
    return gfx::Rect();
  return shelf_widget_->GetScreenBoundsOfItemIconForWindow(window);
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
  ShelfModel* shelf_model = WmShell::Get()->shelf_model();
  const ShelfItem& item = shelf_model->items()[item_index];
  ShelfItemDelegate* item_delegate = shelf_model->GetShelfItemDelegate(item.id);
  item_delegate->ItemSelected(ui::ET_KEY_RELEASED, ui::EF_NONE,
                              display::kInvalidDisplayId, LAUNCH_FROM_UNKNOWN);
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
  for (auto& observer : observers_)
    observer.OnShelfIconPositionsChanged();
}

StatusAreaWidget* WmShelf::GetStatusAreaWidget() const {
  return shelf_widget_->status_area_widget();
}

void WmShelf::SetVirtualKeyboardBoundsForTesting(const gfx::Rect& bounds) {
  shelf_layout_manager_->OnKeyboardBoundsChanging(bounds);
}

ShelfLockingManager* WmShelf::GetShelfLockingManagerForTesting() {
  return shelf_locking_manager_.get();
}

ShelfView* WmShelf::GetShelfViewForTesting() {
  return shelf_view_;
}

void WmShelf::WillDeleteShelfLayoutManager() {
  if (aura::Env::GetInstance()->mode() == aura::Env::Mode::MUS) {
    // TODO(sky): this should be removed once Shell is used everywhere.
    ShutdownShelfWidget();
  }

  // Clear event handlers that might forward events to the destroyed instance.
  auto_hide_event_handler_.reset();
  bezel_event_handler_.reset();

  DCHECK(shelf_layout_manager_);
  shelf_layout_manager_->RemoveObserver(this);
  shelf_layout_manager_ = nullptr;
}

void WmShelf::WillChangeVisibilityState(ShelfVisibilityState new_state) {
  for (auto& observer : observers_)
    observer.WillChangeVisibilityState(new_state);
  if (new_state != SHELF_AUTO_HIDE) {
    auto_hide_event_handler_.reset();
  } else if (!auto_hide_event_handler_ &&
             aura::Env::GetInstance()->mode() == aura::Env::Mode::LOCAL) {
    auto_hide_event_handler_ =
        base::MakeUnique<AutoHideEventHandler>(shelf_layout_manager());
  }
}

void WmShelf::OnAutoHideStateChanged(ShelfAutoHideState new_state) {
  for (auto& observer : observers_)
    observer.OnAutoHideStateChanged(new_state);
}

void WmShelf::OnBackgroundUpdated(ShelfBackgroundType background_type,
                                  AnimationChangeType change_type) {
  if (background_type == GetBackgroundType())
    return;
  for (auto& observer : observers_)
    observer.OnBackgroundTypeChanged(background_type, change_type);
}

}  // namespace ash
