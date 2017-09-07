// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf.h"

#include "ash/public/cpp/config.h"
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
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "ui/app_list/presenter/app_list.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

namespace {

// A callback that does nothing after shelf item selection handling.
void NoopCallback(ShelfAction,
                  base::Optional<std::vector<mojom::MenuItemPtr>>) {}

}  // namespace

// Shelf::AutoHideEventHandler -----------------------------------------------

// Forwards mouse and gesture events to ShelfLayoutManager.
// TODO(mash): Add similar event handling support for mash.
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
    shelf_layout_manager_->ProcessGestureEventOnWindow(
        event, static_cast<aura::Window*>(event->target()));
  }

 private:
  ShelfLayoutManager* shelf_layout_manager_;
  DISALLOW_COPY_AND_ASSIGN(AutoHideEventHandler);
};

// Shelf ---------------------------------------------------------------------

Shelf::Shelf() : shelf_locking_manager_(this) {
  // TODO: ShelfBezelEventHandler needs to work with mus too.
  // http://crbug.com/636647
  if (Shell::GetAshConfig() != Config::MASH)
    bezel_event_handler_ = base::MakeUnique<ShelfBezelEventHandler>(this);
}

Shelf::~Shelf() {}

// static
Shelf* Shelf::ForWindow(aura::Window* window) {
  return RootWindowController::ForWindow(window)->shelf();
}

void Shelf::CreateShelfWidget(aura::Window* root) {
  DCHECK(!shelf_widget_);
  aura::Window* shelf_container =
      root->GetChildById(kShellWindowId_ShelfContainer);
  shelf_widget_.reset(new ShelfWidget(shelf_container, this));

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
  if (shelf_widget_)
    shelf_widget_->Shutdown();
}

void Shelf::DestroyShelfWidget() {
  // May be called multiple times during shutdown.
  shelf_widget_.reset();
}

aura::Window* Shelf::GetWindow() {
  return shelf_widget_->GetNativeWindow();
}

void Shelf::SetAlignment(ShelfAlignment alignment) {
  // Checks added for http://crbug.com/738011.
  CHECK(shelf_widget_);
  CHECK(shelf_layout_manager_);

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
  return shelf_widget_->GetBackgroundType();
}

void Shelf::UpdateVisibilityState() {
  if (shelf_layout_manager_)
    shelf_layout_manager_->UpdateVisibilityState();
}

ShelfVisibilityState Shelf::GetVisibilityState() const {
  return shelf_layout_manager_ ? shelf_layout_manager_->visibility_state()
                               : SHELF_HIDDEN;
}

int Shelf::GetAccessibilityPanelHeight() const {
  return shelf_layout_manager_ ? shelf_layout_manager_->chromevox_panel_height()
                               : 0;
}

gfx::Rect Shelf::GetIdealBounds() {
  return shelf_layout_manager_->GetIdealBounds();
}

gfx::Rect Shelf::GetUserWorkAreaBounds() const {
  return shelf_layout_manager_ ? shelf_layout_manager_->user_work_area_bounds()
                               : gfx::Rect();
}

void Shelf::UpdateIconPositionForPanel(aura::Window* panel) {
  shelf_widget_->UpdateIconPositionForPanel(panel);
}

gfx::Rect Shelf::GetScreenBoundsOfItemIconForWindow(aura::Window* window) {
  if (!shelf_widget_)
    return gfx::Rect();
  return shelf_widget_->GetScreenBoundsOfItemIconForWindow(window);
}

// static
void Shelf::LaunchShelfItem(int item_index) {
  ShelfModel* shelf_model = Shell::Get()->shelf_model();
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
void Shelf::ActivateShelfItem(int item_index) {
  ActivateShelfItemOnDisplay(item_index, display::kInvalidDisplayId);
}

// static
void Shelf::ActivateShelfItemOnDisplay(int item_index, int64_t display_id) {
  ShelfModel* shelf_model = Shell::Get()->shelf_model();
  const ShelfItem& item = shelf_model->items()[item_index];
  ShelfItemDelegate* item_delegate = shelf_model->GetShelfItemDelegate(item.id);
  std::unique_ptr<ui::Event> event = base::MakeUnique<ui::KeyEvent>(
      ui::ET_KEY_RELEASED, ui::VKEY_UNKNOWN, ui::EF_NONE);
  item_delegate->ItemSelected(std::move(event), display_id, LAUNCH_FROM_UNKNOWN,
                              base::Bind(&NoopCallback));
}

bool Shelf::ProcessGestureEvent(const ui::GestureEvent& event) {
  // Can be called at login screen.
  if (!shelf_layout_manager_)
    return false;
  return shelf_layout_manager_->ProcessGestureEvent(event);
}

void Shelf::ProcessMouseWheelEvent(const ui::MouseWheelEvent& event) {
  Shell::Get()->app_list()->ProcessMouseWheelEvent(event);
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
  return shelf_widget_->status_area_widget();
}

void Shelf::SetVirtualKeyboardBoundsForTesting(const gfx::Rect& bounds) {
  shelf_layout_manager_->OnKeyboardBoundsChanging(bounds);
}

ShelfLockingManager* Shelf::GetShelfLockingManagerForTesting() {
  return &shelf_locking_manager_;
}

ShelfView* Shelf::GetShelfViewForTesting() {
  return shelf_widget_->shelf_view_for_testing();
}

void Shelf::WillDeleteShelfLayoutManager() {
  if (Shell::GetAshConfig() == Config::MASH) {
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

void Shelf::WillChangeVisibilityState(ShelfVisibilityState new_state) {
  for (auto& observer : observers_)
    observer.WillChangeVisibilityState(new_state);
  if (new_state != SHELF_AUTO_HIDE) {
    auto_hide_event_handler_.reset();
  } else if (!auto_hide_event_handler_ &&
             Shell::GetAshConfig() != Config::MASH) {
    auto_hide_event_handler_ =
        base::MakeUnique<AutoHideEventHandler>(shelf_layout_manager());
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

}  // namespace ash
