// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf.h"

#include <memory>

#include "ash/animation/animation_change_type.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/keyboard/ui/keyboard_controller_observer.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_bezel_event_handler.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_observer.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/work_area_insets.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

// Shelf::AutoHideEventHandler -----------------------------------------------

// Forwards mouse and gesture events to ShelfLayoutManager for auto-hide.
class Shelf::AutoHideEventHandler : public ui::EventHandler {
 public:
  explicit AutoHideEventHandler(ShelfLayoutManager* shelf_layout_manager)
      : shelf_layout_manager_(shelf_layout_manager) {
    Shell::Get()->AddPreTargetHandler(this);
  }
  ~AutoHideEventHandler() override {
    Shell::Get()->RemovePreTargetHandler(this);
  }

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    shelf_layout_manager_->UpdateAutoHideForMouseEvent(
        event, static_cast<aura::Window*>(event->target()));
  }
  void OnGestureEvent(ui::GestureEvent* event) override {
    shelf_layout_manager_->ProcessGestureEventOfAutoHideShelf(
        event, static_cast<aura::Window*>(event->target()));
  }

 private:
  ShelfLayoutManager* shelf_layout_manager_;
  DISALLOW_COPY_AND_ASSIGN(AutoHideEventHandler);
};

// Shelf ---------------------------------------------------------------------

Shelf::Shelf()
    : shelf_locking_manager_(this),
      bezel_event_handler_(std::make_unique<ShelfBezelEventHandler>(this)) {}

Shelf::~Shelf() = default;

// static
Shelf* Shelf::ForWindow(aura::Window* window) {
  return RootWindowController::ForWindow(window)->shelf();
}

// static
void Shelf::LaunchShelfItem(int item_index) {
  const ShelfModel* shelf_model = ShelfModel::Get();
  const ShelfItems& items = shelf_model->items();
  int item_count = shelf_model->item_count();
  int indexes_left = item_index >= 0 ? item_index : item_count;
  int found_index = -1;

  // Iterating until we have hit the index we are interested in which
  // is true once indexes_left becomes negative.
  for (int i = 0; i < item_count && indexes_left >= 0; i++) {
    if (items[i].type != TYPE_APP_LIST && items[i].type != TYPE_BACK_BUTTON) {
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
void Shelf::ActivateShelfItem(int item_index) {
  ActivateShelfItemOnDisplay(item_index, display::kInvalidDisplayId);
}

// static
void Shelf::ActivateShelfItemOnDisplay(int item_index, int64_t display_id) {
  const ShelfModel* shelf_model = ShelfModel::Get();
  const ShelfItem& item = shelf_model->items()[item_index];
  ShelfItemDelegate* item_delegate = shelf_model->GetShelfItemDelegate(item.id);
  std::unique_ptr<ui::Event> event = std::make_unique<ui::KeyEvent>(
      ui::ET_KEY_RELEASED, ui::VKEY_UNKNOWN, ui::EF_NONE);
  item_delegate->ItemSelected(std::move(event), display_id, LAUNCH_FROM_SHELF,
                              base::DoNothing());
}

void Shelf::CreateShelfWidget(aura::Window* root) {
  DCHECK(!shelf_widget_);
  aura::Window* shelf_container =
      root->GetChildById(kShellWindowId_ShelfContainer);
  shelf_widget_.reset(new ShelfWidget(shelf_container, this));
  shelf_widget_->Initialize();

  DCHECK(!shelf_layout_manager_);
  shelf_layout_manager_ = shelf_widget_->shelf_layout_manager();
  shelf_layout_manager_->AddObserver(this);

  // Must occur after |shelf_widget_| is constructed because the system tray
  // constructors call back into Shelf::shelf_widget().
  DCHECK(!shelf_widget_->status_area_widget());
  aura::Window* status_container =
      root->GetChildById(kShellWindowId_StatusContainer);
  shelf_widget_->CreateStatusAreaWidget(status_container);
}

void Shelf::ShutdownShelfWidget() {
  shelf_widget_->Shutdown();
}

void Shelf::DestroyShelfWidget() {
  DCHECK(shelf_widget_);
  shelf_widget_.reset();
}

bool Shelf::IsVisible() const {
  return shelf_layout_manager_->IsVisible();
}

const aura::Window* Shelf::GetWindow() const {
  return shelf_widget_ ? shelf_widget_->GetNativeWindow() : nullptr;
}

aura::Window* Shelf::GetWindow() {
  return const_cast<aura::Window*>(const_cast<const Shelf*>(this)->GetWindow());
}

void Shelf::SetAlignment(ShelfAlignment alignment) {
  if (!shelf_widget_)
    return;

  if (alignment_ == alignment)
    return;

  if (shelf_locking_manager_.is_locked() &&
      alignment != SHELF_ALIGNMENT_BOTTOM_LOCKED) {
    shelf_locking_manager_.set_stored_alignment(alignment);
    return;
  }

  alignment_ = alignment;
  // The ShelfWidget notifies the ShelfView of the alignment change.
  shelf_widget_->OnShelfAlignmentChanged();
  shelf_layout_manager_->LayoutShelf();
  Shell::Get()->NotifyShelfAlignmentChanged(GetWindow()->GetRootWindow());
}

bool Shelf::IsHorizontalAlignment() const {
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

int Shelf::SelectValueForShelfAlignment(int bottom, int left, int right) const {
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

int Shelf::PrimaryAxisValue(int horizontal, int vertical) const {
  return IsHorizontalAlignment() ? horizontal : vertical;
}

void Shelf::SetAutoHideBehavior(ShelfAutoHideBehavior auto_hide_behavior) {
  DCHECK(shelf_layout_manager_);

  if (auto_hide_behavior_ == auto_hide_behavior)
    return;

  auto_hide_behavior_ = auto_hide_behavior;
  Shell::Get()->NotifyShelfAutoHideBehaviorChanged(
      GetWindow()->GetRootWindow());
}

ShelfAutoHideState Shelf::GetAutoHideState() const {
  return shelf_layout_manager_->auto_hide_state();
}

void Shelf::UpdateAutoHideState() {
  shelf_layout_manager_->UpdateAutoHideState();
}

ShelfBackgroundType Shelf::GetBackgroundType() const {
  return shelf_widget_ ? shelf_widget_->GetBackgroundType()
                       : SHELF_BACKGROUND_DEFAULT;
}

void Shelf::UpdateVisibilityState() {
  if (shelf_layout_manager_)
    shelf_layout_manager_->UpdateVisibilityState();
}

void Shelf::SetSuspendVisibilityUpdate(bool value) {
  if (shelf_layout_manager_)
    shelf_layout_manager_->set_suspend_visibility_update(value);
}

void Shelf::MaybeUpdateShelfBackground() {
  if (!shelf_layout_manager_)
    return;

  shelf_layout_manager_->MaybeUpdateShelfBackground(
      AnimationChangeType::ANIMATE);
}

ShelfVisibilityState Shelf::GetVisibilityState() const {
  return shelf_layout_manager_ ? shelf_layout_manager_->visibility_state()
                               : SHELF_HIDDEN;
}

gfx::Rect Shelf::GetIdealBounds() const {
  return shelf_layout_manager_->GetIdealBounds();
}

gfx::Rect Shelf::GetScreenBoundsOfItemIconForWindow(aura::Window* window) {
  if (!shelf_widget_)
    return gfx::Rect();
  return shelf_widget_->GetScreenBoundsOfItemIconForWindow(window);
}

bool Shelf::ProcessGestureEvent(const ui::GestureEvent& event) {
  // Can be called at login screen.
  if (!shelf_layout_manager_)
    return false;
  return shelf_layout_manager_->ProcessGestureEvent(event);
}

void Shelf::ProcessMouseWheelEvent(const ui::MouseWheelEvent& event) {
  if (Shell::Get()->app_list_controller())
    Shell::Get()->app_list_controller()->ProcessMouseWheelEvent(event);
}

void Shelf::AddObserver(ShelfObserver* observer) {
  observers_.AddObserver(observer);
}

void Shelf::RemoveObserver(ShelfObserver* observer) {
  observers_.RemoveObserver(observer);
}

void Shelf::NotifyShelfIconPositionsChanged() {
  for (auto& observer : observers_)
    observer.OnShelfIconPositionsChanged();
}

StatusAreaWidget* Shelf::GetStatusAreaWidget() const {
  return shelf_widget_ ? shelf_widget_->status_area_widget() : nullptr;
}

TrayBackgroundView* Shelf::GetSystemTrayAnchorView() const {
  return GetStatusAreaWidget()->GetSystemTrayAnchor();
}

gfx::Rect Shelf::GetSystemTrayAnchorRect() const {
  // If status area widget is shown without the shelf, system tray should be
  // aligned above status area widget (shown at the same place as if shelf was
  // visible).
  const WorkAreaInsets* const work_area_insets = GetWorkAreaInsets();
  gfx::Rect work_area = shelf_layout_manager_->IsShowingStatusAreaWithoutShelf()
                            ? work_area_insets->ComputeStableWorkArea()
                            : work_area_insets->user_work_area_bounds();

  switch (alignment_) {
    case SHELF_ALIGNMENT_BOTTOM:
    case SHELF_ALIGNMENT_BOTTOM_LOCKED:
      return gfx::Rect(
          base::i18n::IsRTL() ? work_area.x() : work_area.right() - 1,
          work_area.bottom() - 1, 0, 0);
    case SHELF_ALIGNMENT_LEFT:
      return gfx::Rect(work_area.x(), work_area.bottom() - 1, 0, 0);
    case SHELF_ALIGNMENT_RIGHT:
      return gfx::Rect(work_area.right() - 1, work_area.bottom() - 1, 0, 0);
  }
  NOTREACHED();
  return gfx::Rect();
}

bool Shelf::ShouldHideOnSecondaryDisplay(session_manager::SessionState state) {
  if (Shell::GetPrimaryRootWindowController()->shelf() == this)
    return false;

  return state != session_manager::SessionState::ACTIVE;
}

void Shelf::SetVirtualKeyboardBoundsForTesting(const gfx::Rect& bounds) {
  keyboard::KeyboardStateDescriptor state;
  state.is_visible = !bounds.IsEmpty();
  state.visual_bounds = bounds;
  state.occluded_bounds_in_screen = bounds;
  state.displaced_bounds_in_screen = gfx::Rect();
  WorkAreaInsets* work_area_insets = GetWorkAreaInsets();
  work_area_insets->OnKeyboardVisibilityStateChanged(state.is_visible);
  work_area_insets->OnKeyboardVisibleBoundsChanged(state.visual_bounds);
  work_area_insets->OnKeyboardWorkspaceOccludedBoundsChanged(
      state.occluded_bounds_in_screen);
  work_area_insets->OnKeyboardWorkspaceDisplacingBoundsChanged(
      state.displaced_bounds_in_screen);
  work_area_insets->OnKeyboardAppearanceChanged(state);
}

ShelfLockingManager* Shelf::GetShelfLockingManagerForTesting() {
  return &shelf_locking_manager_;
}

ShelfView* Shelf::GetShelfViewForTesting() {
  return shelf_widget_->shelf_view_for_testing();
}

void Shelf::WillDeleteShelfLayoutManager() {
  // Clear event handlers that might forward events to the destroyed instance.
  auto_hide_event_handler_.reset();
  bezel_event_handler_.reset();

  DCHECK(shelf_layout_manager_);
  shelf_layout_manager_->RemoveObserver(this);
  shelf_layout_manager_ = nullptr;
}

void Shelf::WillChangeVisibilityState(ShelfVisibilityState new_state) {
  for (auto& observer : observers_)
    observer.WillChangeVisibilityState(new_state);
  if (new_state != SHELF_AUTO_HIDE) {
    auto_hide_event_handler_.reset();
  } else if (!auto_hide_event_handler_) {
    auto_hide_event_handler_ =
        std::make_unique<AutoHideEventHandler>(shelf_layout_manager());
  }
}

void Shelf::OnAutoHideStateChanged(ShelfAutoHideState new_state) {
  for (auto& observer : observers_)
    observer.OnAutoHideStateChanged(new_state);
}

void Shelf::OnBackgroundUpdated(ShelfBackgroundType background_type,
                                AnimationChangeType change_type) {
  if (background_type == GetBackgroundType())
    return;
  for (auto& observer : observers_)
    observer.OnBackgroundTypeChanged(background_type, change_type);
}

WorkAreaInsets* Shelf::GetWorkAreaInsets() const {
  const aura::Window* window = GetWindow();
  DCHECK(window);
  return WorkAreaInsets::ForWindow(window->GetRootWindow());
}

}  // namespace ash
