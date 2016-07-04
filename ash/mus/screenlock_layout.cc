// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/screenlock_layout.h"

#include "ash/mus/property_util.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_property.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {
namespace mus {

ScreenlockLayout::ScreenlockLayout(::ui::Window* owner)
    : LayoutManager(owner) {}
ScreenlockLayout::~ScreenlockLayout() {}

// We explicitly don't make assertions about the number of children in this
// layout as the number of children can vary when the application providing the
// screenlock restarts.

void ScreenlockLayout::LayoutWindow(::ui::Window* window) {
  gfx::Rect bounds = owner()->bounds();
  bounds.Inset(-25, -25);
  window->SetBounds(bounds);
}

}  // namespace mus
}  // namespace ash
