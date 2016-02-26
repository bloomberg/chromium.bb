// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/fill_layout.h"

#include "components/mus/public/cpp/window.h"

namespace mash {
namespace wm {

FillLayout::FillLayout(mus::Window* owner) : LayoutManager(owner) {}

FillLayout::~FillLayout() {}

void FillLayout::LayoutWindow(mus::Window* window) {
  window->SetBounds(gfx::Rect(owner()->bounds().size()));
}

}  // namespace wm
}  // namespace mash
