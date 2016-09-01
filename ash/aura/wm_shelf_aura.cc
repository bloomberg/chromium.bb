// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/aura/wm_shelf_aura.h"

#include "ash/aura/wm_window_aura.h"
#include "ash/common/shelf/shelf_layout_manager.h"
#include "ash/shelf/dimmer_view.h"
#include "ash/shelf/shelf_bezel_event_handler.h"
#include "ash/shell.h"

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

WmDimmerView* WmShelfAura::CreateDimmerView(bool disable_animations_for_test) {
  return DimmerView::Create(this, disable_animations_for_test);
}

void WmShelfAura::CreateShelfWidget(WmWindow* root) {
  WmShelf::CreateShelfWidget(root);
  bezel_event_handler_.reset(new ShelfBezelEventHandler(this));
}

void WmShelfAura::WillDeleteShelfLayoutManager() {
  // Clear event handlers that might forward events to the destroyed instance.
  auto_hide_event_handler_.reset();
  bezel_event_handler_.reset();
  WmShelf::WillDeleteShelfLayoutManager();
}

void WmShelfAura::WillChangeVisibilityState(ShelfVisibilityState new_state) {
  WmShelf::WillChangeVisibilityState(new_state);
  if (new_state != SHELF_AUTO_HIDE) {
    auto_hide_event_handler_.reset();
  } else if (!auto_hide_event_handler_) {
    auto_hide_event_handler_.reset(
        new AutoHideEventHandler(shelf_layout_manager()));
  }
}

}  // namespace ash
