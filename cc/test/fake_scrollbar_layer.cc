// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_scrollbar_layer.h"

#include "cc/test/fake_scrollbar_theme_painter.h"
#include "cc/test/fake_web_scrollbar.h"
#include "cc/test/fake_web_scrollbar_theme_geometry.h"

namespace cc {

FakeScrollbarLayer::FakeScrollbarLayer(
    bool paint_during_update, bool has_thumb, int scrolling_layer_id)
    : ScrollbarLayer(
        FakeWebScrollbar::create().PassAs<WebKit::WebScrollbar>(),
        FakeScrollbarThemePainter::Create(paint_during_update)
        .PassAs<ScrollbarThemePainter>(),
        FakeWebScrollbarThemeGeometry::create(has_thumb)
        .PassAs<WebKit::WebScrollbarThemeGeometry>(),
        scrolling_layer_id),
      update_count_(0) {
  setAnchorPoint(gfx::PointF(0, 0));
  setBounds(gfx::Size(1, 1));
  setIsDrawable(true);
}

FakeScrollbarLayer::~FakeScrollbarLayer() {}

void FakeScrollbarLayer::update(
    ResourceUpdateQueue& queue,
    const OcclusionTracker* occlusion,
    RenderingStats& stats) {
  ScrollbarLayer::update(queue, occlusion, stats);
  update_count_++;
}

}  // namespace cc
