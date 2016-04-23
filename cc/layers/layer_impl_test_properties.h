// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_IMPL_TEST_PROPERTIES_H_
#define CC_LAYERS_LAYER_IMPL_TEST_PROPERTIES_H_

#include "cc/layers/layer_position_constraint.h"
#include "ui/gfx/geometry/point3_f.h"

namespace cc {

struct LayerImplTestProperties {
  bool double_sided = true;
  bool force_render_surface = false;
  bool is_container_for_fixed_position_layers = false;
  bool should_flatten_transform = true;
  int num_descendants_that_draw_content = 0;
  LayerPositionConstraint position_constraint;
  gfx::Point3F transform_origin;
};

}  // namespace cc
#endif  // CC_LAYERS_LAYER_IMPL_TEST_PROPERTIES_H_
