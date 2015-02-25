// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_DRAW_PROPERTY_UTILS_H_
#define CC_TREES_DRAW_PROPERTY_UTILS_H_

#include "cc/base/cc_export.h"

namespace gfx {
class Rect;
class Transform;
}  // namespace gfx

namespace cc {

class ClipTree;
class Layer;
class OpacityTree;
class TransformTree;

// Computes combined clips for every node in |clip_tree|. This function requires
// that |transform_tree| has been updated via |ComputeTransforms|.
// TODO(vollick): ComputeClips and ComputeTransforms will eventually need to be
// done on both threads.
void CC_EXPORT
ComputeClips(ClipTree* clip_tree, const TransformTree& transform_tree);

// Computes combined (screen space) transforms for every node in the transform
// tree. This must be done prior to calling |ComputeClips|.
void CC_EXPORT ComputeTransforms(TransformTree* transform_tree);

// Computes the visible content rect for every layer under |root_layer|. The
// visible content rect is the clipped content space rect that will be used for
// recording.
void CC_EXPORT
ComputeVisibleRectsUsingPropertyTrees(Layer* root_layer,
                                      const Layer* page_scale_layer,
                                      float page_scale_factor,
                                      float device_scale_factor,
                                      const gfx::Rect& viewport,
                                      const gfx::Transform& device_transform,
                                      TransformTree* transform_tree,
                                      ClipTree* clip_tree,
                                      OpacityTree* opacity_tree);

}  // namespace cc

#endif  // CC_TREES_DRAW_PROPERTY_UTILS_H_
