// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/render_widget_test.h"

#include "base/basictypes.h"
#include "content/renderer/render_view_impl.h"

namespace content {

const int RenderWidgetTest::kNumBytesPerPixel = 4;
const int RenderWidgetTest::kLargeWidth = 1024;
const int RenderWidgetTest::kLargeHeight = 768;
const int RenderWidgetTest::kSmallWidth = 600;
const int RenderWidgetTest::kSmallHeight = 450;
const int RenderWidgetTest::kTextPositionX = 800;
const int RenderWidgetTest::kTextPositionY = 600;
const uint32 RenderWidgetTest::kRedARGB = 0xFFFF0000;

RenderWidgetTest::RenderWidgetTest() {}

RenderWidget* RenderWidgetTest::widget() {
  return static_cast<RenderViewImpl*>(view_);
}

void RenderWidgetTest::OnResize(const ViewMsg_Resize_Params& params) {
  widget()->OnResize(params);
}

bool RenderWidgetTest::next_paint_is_resize_ack() {
  return widget()->next_paint_is_resize_ack();
}

}  // namespace content
