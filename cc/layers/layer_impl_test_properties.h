// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_IMPL_TEST_PROPERTIES_H_
#define CC_LAYERS_LAYER_IMPL_TEST_PROPERTIES_H_

#include <set>

#include "base/memory/ptr_util.h"
#include "cc/layers/layer_position_constraint.h"
#include "ui/gfx/geometry/point3_f.h"

namespace cc {

class LayerImpl;

struct CC_EXPORT LayerImplTestProperties {
  LayerImplTestProperties();
  ~LayerImplTestProperties();

  bool double_sided;
  bool force_render_surface;
  bool is_container_for_fixed_position_layers;
  bool should_flatten_transform;
  int num_descendants_that_draw_content;
  LayerPositionConstraint position_constraint;
  gfx::Point3F transform_origin;
  LayerImpl* scroll_parent;
  std::unique_ptr<std::set<LayerImpl*>> scroll_children;
  LayerImpl* clip_parent;
  std::unique_ptr<std::set<LayerImpl*>> clip_children;
};

}  // namespace cc

#endif  // CC_LAYERS_LAYER_IMPL_TEST_PROPERTIES_H_
