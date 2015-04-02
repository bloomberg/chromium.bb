// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/window_manager/window_manager_test_util.h"

#include "base/stl_util.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "ui/gfx/geometry/rect.h"

namespace window_manager {

TestView::TestView(int id, const gfx::Rect& rect)
    : target_(new ViewTarget(this)) {
  mojo::ViewPrivate(this).set_id(id);

  mojo::Rect mojo_rect = *mojo::Rect::From(rect);
  SetBounds(mojo_rect);
}

TestView::TestView(int id, const gfx::Rect& rect, View* parent)
    : TestView(id, rect) {
  parent->AddChild(this);
}

TestView::~TestView() {
}

// static
TestView* TestView::Build(int id, const gfx::Rect& rect) {
  return new TestView(id, rect);
}

// static
TestView* TestView::Build(int id, const gfx::Rect& rect, mojo::View* parent) {
  return new TestView(id, rect, parent);
}

}  // namespace window_manager
