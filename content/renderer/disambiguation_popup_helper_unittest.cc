// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/disambiguation_popup_helper.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gfx/size_conversions.h"

// these constants are copied from the implementation class
namespace {
const float kDisambiguationPopupMaxScale = 5.0;
const float kDisambiguationPopupMinScale = 2.0;
}  // unnamed namespace

namespace content {

class DisambiguationPopupHelperUnittest : public testing::Test {
 public:
  DisambiguationPopupHelperUnittest()
      : kViewportSize_(640, 480) { }
 protected:
  const gfx::Size kViewportSize_;
};

TEST_F(DisambiguationPopupHelperUnittest, ClipByViewport) {
  gfx::Rect tap_rect(1000, 1000, 10, 10);
  WebKit::WebVector<WebKit::WebRect> target_rects(static_cast<size_t>(1));
  target_rects[0] = gfx::Rect(-20, -20, 10, 10);

  gfx::Rect zoom_rect;
  float scale = DisambiguationPopupHelper::ComputeZoomAreaAndScaleFactor(
      tap_rect, target_rects, kViewportSize_, &zoom_rect);

  EXPECT_TRUE(gfx::Rect(kViewportSize_).Contains(zoom_rect));
  EXPECT_LE(kDisambiguationPopupMinScale, scale);

  gfx::Size scaled_size = ToCeiledSize(ScaleSize(zoom_rect.size(), scale));
  EXPECT_TRUE(gfx::Rect(kViewportSize_).Contains(gfx::Rect(scaled_size)));
}

TEST_F(DisambiguationPopupHelperUnittest, MiniTarget) {
  gfx::Rect tap_rect(-5, -5, 20, 20);
  WebKit::WebVector<WebKit::WebRect> target_rects(static_cast<size_t>(1));
  target_rects[0] = gfx::Rect(10, 10, 1, 1);

  gfx::Rect zoom_rect;
  float scale = DisambiguationPopupHelper::ComputeZoomAreaAndScaleFactor(
      tap_rect, target_rects, kViewportSize_, &zoom_rect);

  EXPECT_TRUE(gfx::Rect(kViewportSize_).Contains(zoom_rect));
  EXPECT_EQ(kDisambiguationPopupMaxScale, scale);
  EXPECT_TRUE(zoom_rect.Contains(target_rects[0]));

  gfx::Size scaled_size = ToCeiledSize(ScaleSize(zoom_rect.size(), scale));
  EXPECT_TRUE(gfx::Rect(kViewportSize_).Contains(gfx::Rect(scaled_size)));
}

TEST_F(DisambiguationPopupHelperUnittest, LongLinks) {
  gfx::Rect tap_rect(10, 10, 20, 20);
  WebKit::WebVector<WebKit::WebRect> target_rects(static_cast<size_t>(2));
  target_rects[0] = gfx::Rect(15, 15, 1000, 5);
  target_rects[1] = gfx::Rect(15, 25, 1000, 5);

  gfx::Rect zoom_rect;
  float scale = DisambiguationPopupHelper::ComputeZoomAreaAndScaleFactor(
      tap_rect, target_rects, kViewportSize_, &zoom_rect);

  EXPECT_TRUE(gfx::Rect(kViewportSize_).Contains(zoom_rect));
  EXPECT_EQ(kDisambiguationPopupMaxScale, scale);
  EXPECT_TRUE(zoom_rect.Contains(tap_rect));

  gfx::Size scaled_size = ToCeiledSize(ScaleSize(zoom_rect.size(), scale));
  EXPECT_TRUE(gfx::Rect(kViewportSize_).Contains(gfx::Rect(scaled_size)));
}

}  // namespace content
