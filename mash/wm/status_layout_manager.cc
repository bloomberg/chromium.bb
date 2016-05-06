// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/status_layout_manager.h"

#include "components/mus/public/cpp/window.h"
#include "mash/wm/property_util.h"
#include "mash/wm/public/interfaces/ash_window_type.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace mash {
namespace wm {

StatusLayoutManager::StatusLayoutManager(mus::Window* owner)
    : LayoutManager(owner),
      alignment_(mash::shelf::mojom::Alignment::BOTTOM),
      auto_hide_behavior_(mash::shelf::mojom::AutoHideBehavior::NEVER) {
  AddLayoutProperty(mus::mojom::WindowManager::kPreferredSize_Property);
}

StatusLayoutManager::~StatusLayoutManager() {}

// We explicitly don't make assertions about the number of children in this
// layout as the number of children can vary when the application providing the
// status area restarts.

void StatusLayoutManager::LayoutWindow(mus::Window* window) {
  if (GetAshWindowType(window) != mojom::AshWindowType::STATUS_AREA) {
    // TODO(jamescook): Layout for notifications and other windows.
    NOTIMPLEMENTED() << "Non-status-area window needs layout.";
    return;
  }
  gfx::Size size = GetWindowPreferredSize(window);
  if (alignment_ == mash::shelf::mojom::Alignment::BOTTOM) {
    const int y = owner()->bounds().height() - size.height();
    // TODO(msw): Place the status area widget on the left for RTL UIs.
    window->SetBounds(gfx::Rect(owner()->bounds().width() - size.width(), y,
                                size.width(), size.height()));
  } else {
    const int x = (alignment_ == mash::shelf::mojom::Alignment::LEFT)
                      ? 0
                      : (owner()->bounds().width() - size.width());
    window->SetBounds(gfx::Rect(x, owner()->bounds().height() - size.height(),
                                size.width(), size.height()));
  }
}

void StatusLayoutManager::SetAlignment(
    mash::shelf::mojom::Alignment alignment) {
  if (alignment_ == alignment)
    return;

  alignment_ = alignment;
  for (mus::Window* window : owner()->children())
    LayoutWindow(window);
}

void StatusLayoutManager::SetAutoHideBehavior(
    mash::shelf::mojom::AutoHideBehavior auto_hide) {
  if (auto_hide_behavior_ == auto_hide)
    return;

  auto_hide_behavior_ = auto_hide;
  NOTIMPLEMENTED();
}

}  // namespace wm
}  // namespace mash
