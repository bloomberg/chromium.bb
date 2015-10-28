// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/example/wm/background_layout.h"

#include "components/mus/public/cpp/window.h"

BackgroundLayout::BackgroundLayout(mus::Window* owner) : LayoutManager(owner) {}
BackgroundLayout::~BackgroundLayout() {}

void BackgroundLayout::WindowAdded(mus::Window* window) {
  DCHECK_EQ(owner()->children().size(), 1U);
}

void BackgroundLayout::LayoutWindow(mus::Window* window) {
  window->SetBounds(gfx::Rect(owner()->bounds().size()));
}
