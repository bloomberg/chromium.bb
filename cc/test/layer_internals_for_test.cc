// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/layer_internals_for_test.h"

#include "cc/layers/layer.h"

namespace cc {

LayerInternalsForTest::LayerInternalsForTest(Layer* layer) : layer_(layer) {}

void LayerInternalsForTest::OnOpacityAnimated(float opacity) {
  layer_->OnOpacityAnimated(opacity);
}

void LayerInternalsForTest::OnTransformAnimated(
    const gfx::Transform& transform) {
  layer_->OnTransformAnimated(transform);
}

void LayerInternalsForTest::OnScrollOffsetAnimated(
    const gfx::ScrollOffset& scroll_offset) {
  layer_->OnScrollOffsetAnimated(scroll_offset);
}

}  // namespace cc
