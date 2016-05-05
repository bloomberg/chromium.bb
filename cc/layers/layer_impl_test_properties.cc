// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_impl_test_properties.h"

namespace cc {

LayerImplTestProperties::LayerImplTestProperties()
    : double_sided(true),
      force_render_surface(false),
      is_container_for_fixed_position_layers(false),
      should_flatten_transform(true),
      hide_layer_and_subtree(false),
      num_descendants_that_draw_content(0),
      num_unclipped_descendants(0),
      scroll_parent(nullptr),
      clip_parent(nullptr) {}

LayerImplTestProperties::~LayerImplTestProperties() {}

}  // namespace cc
