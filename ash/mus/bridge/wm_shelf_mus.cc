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

wm::WmWindow* WmShelfMus::GetWindow() {
  return WmWindowMus::Get(shelf_layout_manager_->GetShelfWindow());
}

wm::ShelfAlignment WmShelfMus::GetAlignment() const {
  switch (shelf_layout_manager_->alignment()) {
    case mash::shelf::mojom::Alignment::BOTTOM:
      return wm::SHELF_ALIGNMENT_BOTTOM;
    case mash::shelf::mojom::Alignment::LEFT:
      return wm::SHELF_ALIGNMENT_LEFT;
    case mash::shelf::mojom::Alignment::RIGHT:
      return wm::SHELF_ALIGNMENT_RIGHT;
  }
  NOTREACHED();
  return wm::SHELF_ALIGNMENT_BOTTOM;
}

wm::ShelfBackgroundType WmShelfMus::GetBackgroundType() const {
  NOTIMPLEMENTED();
  return wm::SHELF_BACKGROUND_DEFAULT;
}

void WmShelfMus::UpdateVisibilityState() {
  NOTIMPLEMENTED();
}

ShelfVisibilityState WmShelfMus::GetVisibilityState() const {
  NOTIMPLEMENTED();
  return shelf_layout_manager_->GetShelfWindow() ? SHELF_VISIBLE : SHELF_HIDDEN;
}

void WmShelfMus::UpdateIconPositionForWindow(wm::WmWindow* window) {
  NOTIMPLEMENTED();
}

gfx::Rect WmShelfMus::GetScreenBoundsOfItemIconForWindow(wm::WmWindow* window) {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

void WmShelfMus::AddObserver(wm::WmShelfObserver* observer) {
  observers_.AddObserver(observer);
}

void WmShelfMus::RemoveObserver(wm::WmShelfObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace mus
}  // namespace ash
