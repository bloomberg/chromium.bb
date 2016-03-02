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

ShelfLayout::ShelfLayout(mus::Window* owner) : LayoutManager(owner) {
  AddLayoutProperty(mus::mojom::WindowManager::kPreferredSize_Property);
}

ShelfLayout::~ShelfLayout() {}

// We explicitly don't make assertions about the number of children in this
// layout as the number of children can vary when the application providing the
// shelf restarts.

void ShelfLayout::LayoutWindow(mus::Window* window) {
  // TODO(msw): Support additional shelf alignments and RTL UI.
  gfx::Size size = GetWindowPreferredSize(window);
  const int y = owner()->bounds().height() - size.height();
  const mojom::AshWindowType ash_window_type = GetAshWindowType(window);
  if (ash_window_type == mojom::AshWindowType::SHELF) {
    size.set_width(owner()->bounds().width());
    window->SetBounds(gfx::Rect(0, y, size.width(), size.height()));
  } else if (ash_window_type == mojom::AshWindowType::STATUS_AREA) {
    window->SetBounds(gfx::Rect(owner()->bounds().width() - size.width(), y,
                                size.width(), size.height()));
  } else {
    NOTREACHED() << "Unknown window in USER_SHELF container.";
  }
}

}  // namespace wm
}  // namespace mash
