// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/aura/wm_shelf_aura.h"

#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/wm/common/shelf/wm_shelf_observer.h"
#include "ash/wm/common/wm_window.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace wm {

WmShelfAura::WmShelfAura(Shelf* shelf)
    : shelf_(shelf), shelf_layout_manager_(shelf->shelf_layout_manager()) {
  shelf_layout_manager_->AddObserver(this);
}

WmShelfAura::~WmShelfAura() {
  if (shelf_layout_manager_)
    shelf_layout_manager_->RemoveObserver(this);
}

WmWindow* WmShelfAura::GetWindow() {
  return WmWindow::Get(shelf_->shelf_widget());
}

ShelfAlignment WmShelfAura::GetAlignment() {
  return shelf_->alignment();
}

ShelfBackgroundType WmShelfAura::GetBackgroundType() {
  return shelf_->shelf_widget()->GetBackgroundType();
}

void WmShelfAura::UpdateVisibilityState() {
  shelf_->shelf_layout_manager()->UpdateVisibilityState();
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

}  // namespace wm
}  // namespace ash
