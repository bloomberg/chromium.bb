// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_scrollbar_layer.h"

#include "base/auto_reset.h"
#include "cc/resources/resource_update_queue.h"
#include "cc/test/fake_scrollbar.h"

namespace cc {

FakeScrollbarLayer::FakeScrollbarLayer(bool paint_during_update,
                                       bool has_thumb,
                                       int scrolling_layer_id)
    : ScrollbarLayer(
          scoped_ptr<Scrollbar>(
              new FakeScrollbar(paint_during_update, has_thumb, false)).Pass(),
          scrolling_layer_id),
      update_count_(0),
      push_properties_count_(0),
      last_update_full_upload_size_(0),
      last_update_partial_upload_size_(0) {
  SetAnchorPoint(gfx::PointF(0.f, 0.f));
  SetBounds(gfx::Size(1, 1));
  SetIsDrawable(true);
}

FakeScrollbarLayer::~FakeScrollbarLayer() {}

bool FakeScrollbarLayer::Update(ResourceUpdateQueue* queue,
                                const OcclusionTracker* occlusion) {
  size_t full = queue->FullUploadSize();
  size_t partial = queue->PartialUploadSize();
  bool updated = ScrollbarLayer::Update(queue, occlusion);
  update_count_++;
  last_update_full_upload_size_ = queue->FullUploadSize() - full;
  last_update_partial_upload_size_ = queue->PartialUploadSize() - partial;
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
