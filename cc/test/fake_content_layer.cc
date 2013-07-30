// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_content_layer.h"

#include "cc/resources/prioritized_resource.h"
#include "cc/test/fake_content_layer_impl.h"

namespace cc {

FakeContentLayer::FakeContentLayer(ContentLayerClient* client)
    : ContentLayer(client),
      update_count_(0),
      push_properties_count_(0),
      always_update_resources_(false) {
  SetAnchorPoint(gfx::PointF(0.f, 0.f));
  SetBounds(gfx::Size(1, 1));
  SetIsDrawable(true);
}

FakeContentLayer::~FakeContentLayer() {}

scoped_ptr<LayerImpl> FakeContentLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return FakeContentLayerImpl::Create(tree_impl, layer_id_).PassAs<LayerImpl>();
}

bool FakeContentLayer::Update(ResourceUpdateQueue* queue,
                              const OcclusionTracker* occlusion) {
  bool updated = ContentLayer::Update(queue, occlusion);
  update_count_++;
  return updated || always_update_resources_;
}

void FakeContentLayer::PushPropertiesTo(LayerImpl* layer) {
  ContentLayer::PushPropertiesTo(layer);
  push_properties_count_++;
}

bool FakeContentLayer::HaveBackingAt(int i, int j) {
  const PrioritizedResource* resource = ResourceAtForTesting(i, j);
  return resource && resource->have_backing_texture();
}

}  // namespace cc
