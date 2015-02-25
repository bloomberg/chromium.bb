// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_PROPERTY_TREE_BUILDER_H_
#define CC_TREES_PROPERTY_TREE_BUILDER_H_

#include <vector>

#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/property_tree.h"

namespace cc {

class LayerTreeHost;

class PropertyTreeBuilder {
 public:
  // Building an opacity tree is optional, and can be skipped by passing
  // in a null |opacity_tree|.
  static void BuildPropertyTrees(Layer* root_layer,
                                 const Layer* page_scale_layer,
                                 float page_scale_factor,
                                 float device_scale_factor,
                                 const gfx::Rect& viewport,
                                 const gfx::Transform& device_transform,
                                 TransformTree* transform_tree,
                                 ClipTree* clip_tree,
                                 OpacityTree* opacity_tree);
};

}  // namespace cc

#endif  // CC_TREES_PROPERTY_TREE_BUILDER_H_
