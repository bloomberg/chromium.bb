// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/shelf_layout_manager.h"

#include "components/mus/public/cpp/window.h"
#include "mash/wm/property_util.h"
#include "mash/wm/public/interfaces/ash_window_type.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace mash {
namespace wm {

ShelfLayoutManager::ShelfLayoutManager(mus::Window* owner)
    : LayoutManager(owner),
      alignment_(mash::shelf::mojom::Alignment::BOTTOM),
      auto_hide_behavior_(mash::shelf::mojom::AutoHideBehavior::NEVER) {
  AddLayoutProperty(mus::mojom::WindowManager::kPreferredSize_Property);
}

ShelfLayoutManager::~ShelfLayoutManager() {}

// We explicitly don't make assertions about the number of children in this
// layout as the number of children can vary when the application providing the
// shelf restarts.

void ShelfLayoutManager::LayoutWindow(mus::Window* window) {
  if (GetAshWindowType(window) != mojom::AshWindowType::SHELF) {
    NOTREACHED() << "Unknown window in USER_SHELF container.";
    return;
  }
  gfx::Size size = GetWindowPreferredSize(window);

  if (alignment_ == mash::shelf::mojom::Alignment::BOTTOM) {
    const int y = owner()->bounds().height() - size.height();
    size.set_width(owner()->bounds().width());
    window->SetBounds(gfx::Rect(0, y, size.width(), size.height()));
  } else {
    const int x = (alignment_ == mash::shelf::mojom::Alignment::LEFT)
                      ? 0
                      : (owner()->bounds().width() - size.width());
    size.set_height(owner()->bounds().height());
    window->SetBounds(gfx::Rect(x, 0, size.width(), size.height()));
  }
}

void ShelfLayoutManager::SetAlignment(mash::shelf::mojom::Alignment alignment) {
  if (alignment_ == alignment)
    return;

  alignment_ = alignment;
  for (mus::Window* window : owner()->children())
    LayoutWindow(window);
}

void ShelfLayoutManager::SetAutoHideBehavior(
    mash::shelf::mojom::AutoHideBehavior auto_hide) {
  if (auto_hide_behavior_ == auto_hide)
    return;

  auto_hide_behavior_ = auto_hide;
  NOTIMPLEMENTED();
}

}  // namespace wm
}  // namespace mash
