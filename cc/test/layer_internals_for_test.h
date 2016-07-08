// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_LAYER_INTERNALS_FOR_TEST_H_
#define CC_TEST_LAYER_INTERNALS_FOR_TEST_H_

#include "base/macros.h"
#include "cc/layers/layer.h"

namespace cc {

// Utility class to give tests access to Layer private methods.
class LayerInternalsForTest {
 public:
  explicit LayerInternalsForTest(Layer* layer);

  void OnOpacityAnimated(float opacity);
  void OnTransformAnimated(const gfx::Transform& transform);
  void OnScrollOffsetAnimated(const gfx::ScrollOffset& scroll_offset);

 private:
  Layer* layer_;
};

}  // namespace cc

#endif  // CC_TEST_LAYER_INTERNALS_FOR_TEST_H_
