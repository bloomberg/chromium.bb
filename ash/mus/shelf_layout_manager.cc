// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/shelf_layout_manager.h"

#include "ash/mus/property_util.h"
#include "ash/mus/shelf_layout_manager_delegate.h"
#include "ash/public/interfaces/ash_window_type.mojom.h"
#include "components/mus/public/cpp/window.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {
namespace mus {

ShelfLayoutManager::ShelfLayoutManager(::mus::Window* owner,
                                       ShelfLayoutManagerDelegate* delegate)
    : LayoutManager(owner),
      delegate_(delegate),
      alignment_(mash::shelf::mojom::Alignment::BOTTOM),
      auto_hide_behavior_(mash::shelf::mojom::AutoHideBehavior::NEVER) {
  AddLayoutProperty(::mus::mojom::WindowManager::kPreferredSize_Property);
}

ShelfLayoutManager::~ShelfLayoutManager() {}

::mus::Window* ShelfLayoutManager::GetShelfWindow() {
  for (::mus::Window* child : owner()->children()) {
    if (GetAshWindowType(child) == mojom::AshWindowType::SHELF)
      return child;
  }
  return nullptr;
}

void ShelfLayoutManager::SetAlignment(mash::shelf::mojom::Alignment alignment) {
  if (alignment_ == alignment)
    return;

  alignment_ = alignment;
  for (::mus::Window* window : owner()->children())
    LayoutWindow(window);
}

void ShelfLayoutManager::SetAutoHideBehavior(
    mash::shelf::mojom::AutoHideBehavior auto_hide) {
  if (auto_hide_behavior_ == auto_hide)
    return;

  auto_hide_behavior_ = auto_hide;
  NOTIMPLEMENTED();
}

// We explicitly don't make assertions about the number of children in this
// layout as the number of children can vary when the application providing the
// shelf restarts.

void ShelfLayoutManager::LayoutWindow(::mus::Window* window) {
  if (GetAshWindowType(window) != mojom::AshWindowType::SHELF) {
    // Phantom windows end up in this container, ignore them.
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

void ShelfLayoutManager::WindowAdded(::mus::Window* window) {
  if (GetAshWindowType(window) == mojom::AshWindowType::SHELF)
    delegate_->OnShelfWindowAvailable();
}

}  // namespace mus
}  // namespace ash
