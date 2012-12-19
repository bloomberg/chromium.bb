// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_content_layer.h"

namespace cc {

FakeContentLayer::FakeContentLayer(ContentLayerClient* client)
    : ContentLayer(client),
      update_count_(0) {
  setAnchorPoint(gfx::PointF(0, 0));
  setBounds(gfx::Size(1, 1));
  setIsDrawable(true);
}

FakeContentLayer::~FakeContentLayer() {}

void FakeContentLayer::update(
    ResourceUpdateQueue& queue,
    const OcclusionTracker* occlusion,
    RenderingStats& stats) {
  ContentLayer::update(queue, occlusion, stats);
  update_count_++;
}

}  // namespace cc
