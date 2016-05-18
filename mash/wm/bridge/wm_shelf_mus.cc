// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/bridge/wm_shelf_mus.h"

#include "components/mus/public/cpp/window.h"
#include "mash/wm/bridge/wm_window_mus.h"
#include "mash/wm/shelf_layout_manager.h"

// TODO(sky): fully implement this http://crbug.com/612631 .
#undef NOTIMPLEMENTED
#define NOTIMPLEMENTED() DVLOG(1) << "notimplemented"

namespace mash {
namespace wm {

WmShelfMus::WmShelfMus(ShelfLayoutManager* shelf_layout_manager)
    : shelf_layout_manager_(shelf_layout_manager) {}

WmShelfMus::~WmShelfMus() {}

ash::wm::WmWindow* WmShelfMus::GetWindow() {
  return WmWindowMus::Get(shelf_layout_manager_->GetShelfWindow());
}

ash::wm::ShelfAlignment WmShelfMus::GetAlignment() const {
  switch (shelf_layout_manager_->alignment()) {
    case shelf::mojom::Alignment::BOTTOM:
      return ash::wm::SHELF_ALIGNMENT_BOTTOM;
    case shelf::mojom::Alignment::LEFT:
      return ash::wm::SHELF_ALIGNMENT_LEFT;
    case shelf::mojom::Alignment::RIGHT:
      return ash::wm::SHELF_ALIGNMENT_RIGHT;
  }
  NOTREACHED();
  return ash::wm::SHELF_ALIGNMENT_BOTTOM;
}

ash::wm::ShelfBackgroundType WmShelfMus::GetBackgroundType() const {
  NOTIMPLEMENTED();
  return ash::wm::SHELF_BACKGROUND_DEFAULT;
}

void WmShelfMus::UpdateVisibilityState() {
  NOTIMPLEMENTED();
}

ash::ShelfVisibilityState WmShelfMus::GetVisibilityState() const {
  NOTIMPLEMENTED();
  return shelf_layout_manager_->GetShelfWindow() ? ash::SHELF_VISIBLE
                                                 : ash::SHELF_HIDDEN;
}

void WmShelfMus::UpdateIconPositionForWindow(ash::wm::WmWindow* window) {
  NOTIMPLEMENTED();
}

gfx::Rect WmShelfMus::GetScreenBoundsOfItemIconForWindow(
    ash::wm::WmWindow* window) {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

void WmShelfMus::AddObserver(ash::wm::WmShelfObserver* observer) {
  observers_.AddObserver(observer);
}

void WmShelfMus::RemoveObserver(ash::wm::WmShelfObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace wm
}  // namespace mash
