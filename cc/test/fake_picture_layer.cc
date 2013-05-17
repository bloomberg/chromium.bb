// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_picture_layer.h"

#include "cc/test/fake_picture_layer_impl.h"

namespace cc {

FakePictureLayer::FakePictureLayer(ContentLayerClient* client)
    : PictureLayer(client),
      update_count_(0) {
  SetAnchorPoint(gfx::PointF(0.f, 0.f));
  SetBounds(gfx::Size(1, 1));
  SetIsDrawable(true);
}

FakePictureLayer::~FakePictureLayer() {}

scoped_ptr<LayerImpl> FakePictureLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return FakePictureLayerImpl::Create(tree_impl, layer_id_).PassAs<LayerImpl>();
}

void FakePictureLayer::Update(ResourceUpdateQueue* queue,
                              const OcclusionTracker* occlusion,
                              RenderingStats* stats) {
  PictureLayer::Update(queue, occlusion, stats);
  update_count_++;
}

}  // namespace cc
