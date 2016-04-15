// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/shelf_layout.h"

#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_property.h"
#include "mash/wm/property_util.h"
#include "mash/wm/public/interfaces/ash_window_type.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace mash {
namespace wm {

namespace {

mojom::AshWindowType GetAshWindowType(const mus::Window* window) {
  if (!window->HasSharedProperty(mojom::kAshWindowType_Property))
    return mojom::AshWindowType::COUNT;

  return static_cast<mojom::AshWindowType>(
      window->GetSharedProperty<int32_t>(mojom::kAshWindowType_Property));
}

}  // namespace

ShelfLayout::ShelfLayout(mus::Window* owner)
    : LayoutManager(owner),
      alignment_(mash::shelf::mojom::Alignment::BOTTOM),
      auto_hide_behavior_(mash::shelf::mojom::AutoHideBehavior::NEVER) {
  AddLayoutProperty(mus::mojom::WindowManager::kPreferredSize_Property);
}

ShelfLayout::~ShelfLayout() {}

// We explicitly don't make assertions about the number of children in this
// layout as the number of children can vary when the application providing the
// shelf restarts.

void ShelfLayout::LayoutWindow(mus::Window* window) {
  const mojom::AshWindowType ash_window_type = GetAshWindowType(window);
  gfx::Size size = GetWindowPreferredSize(window);

  if (alignment_ == mash::shelf::mojom::Alignment::BOTTOM) {
    const int y = owner()->bounds().height() - size.height();
    if (ash_window_type == mojom::AshWindowType::SHELF) {
      size.set_width(owner()->bounds().width());
      window->SetBounds(gfx::Rect(0, y, size.width(), size.height()));
    } else if (ash_window_type == mojom::AshWindowType::STATUS_AREA) {
      // TODO(msw): Place the status area on the left for RTL UIs.
      window->SetBounds(gfx::Rect(owner()->bounds().width() - size.width(), y,
                                  size.width(), size.height()));
    } else {
      NOTREACHED() << "Unknown window in USER_SHELF container.";
    }
  } else {
    const int x = (alignment_ == mash::shelf::mojom::Alignment::LEFT)
                      ? 0
                      : (owner()->bounds().width() - size.width());
    if (ash_window_type == mojom::AshWindowType::SHELF) {
      size.set_height(owner()->bounds().height());
      window->SetBounds(gfx::Rect(x, 0, size.width(), size.height()));
    } else if (ash_window_type == mojom::AshWindowType::STATUS_AREA) {
      window->SetBounds(gfx::Rect(x, owner()->bounds().height() - size.height(),
                                  size.width(), size.height()));
    } else {
      NOTREACHED() << "Unknown window in USER_SHELF container.";
    }
  }
}

void ShelfLayout::SetAlignment(mash::shelf::mojom::Alignment alignment) {
  if (alignment_ == alignment)
    return;

  alignment_ = alignment;
  for (mus::Window* window : owner()->children())
    LayoutWindow(window);
}

void ShelfLayout::SetAutoHideBehavior(
    mash::shelf::mojom::AutoHideBehavior auto_hide) {
  if (auto_hide_behavior_ == auto_hide)
    return;

  auto_hide_behavior_ = auto_hide;
  NOTIMPLEMENTED();
}

}  // namespace wm
}  // namespace mash
