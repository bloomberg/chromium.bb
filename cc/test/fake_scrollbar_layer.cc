// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_scrollbar_layer.h"

#include "cc/resources/resource_update_queue.h"
#include "cc/test/fake_scrollbar_theme_painter.h"
#include "cc/test/fake_web_scrollbar.h"
#include "cc/test/fake_web_scrollbar_theme_geometry.h"

namespace cc {

FakeScrollbarLayer::FakeScrollbarLayer(bool paint_during_update,
                                       bool has_thumb,
                                       int scrolling_layer_id)
    : ScrollbarLayer(FakeWebScrollbar::Create().PassAs<WebKit::WebScrollbar>(),
                     FakeScrollbarThemePainter::Create(paint_during_update).
                         PassAs<ScrollbarThemePainter>(),
                     FakeWebScrollbarThemeGeometry::Create(has_thumb).
                         PassAs<WebKit::WebScrollbarThemeGeometry>(),
                     scrolling_layer_id),
      update_count_(0),
      last_update_full_upload_size_(0),
      last_update_partial_upload_size_(0) {
  SetAnchorPoint(gfx::PointF(0.f, 0.f));
  SetBounds(gfx::Size(1, 1));
  SetIsDrawable(true);
}

FakeScrollbarLayer::~FakeScrollbarLayer() {}

void FakeScrollbarLayer::Update(ResourceUpdateQueue* queue,
                                const OcclusionTracker* occlusion,
                                RenderingStats* stats) {
  size_t full = queue->FullUploadSize();
  size_t partial = queue->PartialUploadSize();
  ScrollbarLayer::Update(queue, occlusion, stats);
  update_count_++;
  last_update_full_upload_size_ = queue->FullUploadSize() - full;
  last_update_partial_upload_size_ = queue->PartialUploadSize() - partial;
}

}  // namespace cc
