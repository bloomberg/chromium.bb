// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_content_layer.h"

#include "cc/prioritized_resource.h"
#include "cc/test/fake_content_layer_impl.h"

namespace cc {

FakeContentLayer::FakeContentLayer(ContentLayerClient* client)
    : ContentLayer(client),
      update_count_(0) {
  setAnchorPoint(gfx::PointF(0, 0));
  setBounds(gfx::Size(1, 1));
  setIsDrawable(true);
}

FakeContentLayer::~FakeContentLayer() {}

scoped_ptr<LayerImpl> FakeContentLayer::createLayerImpl(
    LayerTreeImpl* tree_impl) {
  return FakeContentLayerImpl::Create(tree_impl, m_layerId).PassAs<LayerImpl>();
}

void FakeContentLayer::update(
    ResourceUpdateQueue& queue,
    const OcclusionTracker* occlusion,
    RenderingStats& stats) {
  ContentLayer::update(queue, occlusion, stats);
  update_count_++;
}

bool FakeContentLayer::HaveBackingAt(int i, int j) {
  const PrioritizedResource* resource = resourceAtForTesting(i, j);
  return resource && resource->haveBackingTexture();
}

}  // namespace cc
