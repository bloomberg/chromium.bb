// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/background_layout.h"

#include "components/mus/public/cpp/window.h"

namespace mash {
namespace wm {

BackgroundLayout::BackgroundLayout(mus::Window* owner) : LayoutManager(owner) {}
BackgroundLayout::~BackgroundLayout() {}

// We explicitly don't make assertions about the number of children in this
// layout as the number of children can vary when the application providing the
// background restarts.

void BackgroundLayout::LayoutWindow(mus::Window* window) {
  window->SetBounds(gfx::Rect(owner()->bounds().size()));
}

}  // namespace wm
}  // namespace mash
