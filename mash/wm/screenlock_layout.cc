// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/screenlock_layout.h"

#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_property.h"
#include "mash/wm/property_util.h"
#include "ui/gfx/geometry/rect.h"

namespace mash {
namespace wm {

ScreenlockLayout::ScreenlockLayout(mus::Window* owner) : LayoutManager(owner) {}
ScreenlockLayout::~ScreenlockLayout() {}

// We explicitly don't make assertions about the number of children in this
// layout as the number of children can vary when the application providing the
// screenlock restarts.

void ScreenlockLayout::LayoutWindow(mus::Window* window) {
  window->SetBounds(owner()->bounds());
}

}  // namespace wm
}  // namespace mash
