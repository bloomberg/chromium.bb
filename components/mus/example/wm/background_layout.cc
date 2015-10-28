// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/example/wm/background_layout.h"

#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_property.h"

BackgroundLayout::BackgroundLayout(mus::Window* owner) : LayoutManager(owner) {}
BackgroundLayout::~BackgroundLayout() {}

void BackgroundLayout::WindowAdded(mus::Window* window) {
  DCHECK_EQ(owner()->children().size(), 1U);
}

void BackgroundLayout::LayoutWindow(mus::Window* window) {
  mojo::Rect container_bounds = owner()->bounds();
  container_bounds.x = 0;
  container_bounds.y = 0;
  window->SetBounds(container_bounds);
}
