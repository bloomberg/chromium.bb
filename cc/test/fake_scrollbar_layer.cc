// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_scrollbar_layer.h"

#include "base/auto_reset.h"
#include "cc/resources/resource_update_queue.h"
#include "cc/test/fake_scrollbar.h"

namespace cc {

scoped_refptr<FakeScrollbarLayer> FakeScrollbarLayer::Create(
    bool paint_during_update,
    bool has_thumb,
    int scrolling_layer_id) {
  FakeScrollbar* fake_scrollbar = new FakeScrollbar(
      paint_during_update, has_thumb, false);
  return make_scoped_refptr(new FakeScrollbarLayer(
      fake_scrollbar, scrolling_layer_id));
}

FakeScrollbarLayer::FakeScrollbarLayer(FakeScrollbar* fake_scrollbar,
                                       int scrolling_layer_id)
    : ScrollbarLayer(
          scoped_ptr<Scrollbar>(fake_scrollbar).Pass(),
          scrolling_layer_id),
      update_count_(0),
      push_properties_count_(0),
      fake_scrollbar_(fake_scrollbar) {
  SetAnchorPoint(gfx::PointF(0.f, 0.f));
  SetBounds(gfx::Size(1, 1));
  SetIsDrawable(true);
}

FakeScrollbarLayer::~FakeScrollbarLayer() {}

bool FakeScrollbarLayer::Update(ResourceUpdateQueue* queue,
                                const OcclusionTracker* occlusion) {
  bool updated = ScrollbarLayer::Update(queue, occlusion);
  ++update_count_;
  return updated;
}

void FakeScrollbarLayer::PushPropertiesTo(LayerImpl* layer) {
  ScrollbarLayer::PushPropertiesTo(layer);
  ++push_properties_count_;
}

scoped_ptr<base::AutoReset<bool> > FakeScrollbarLayer::IgnoreSetNeedsCommit() {
  return make_scoped_ptr(
      new base::AutoReset<bool>(&ignore_set_needs_commit_, true));
}

}  // namespace cc
