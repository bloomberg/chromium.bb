// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/render_widget_compositor.h"

#include "cc/output/begin_frame_args.h"
#include "content/public/test/mock_render_thread.h"
#include "content/renderer/render_widget.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"

using testing::AllOf;
using testing::Field;

namespace content {

class MockWebWidget : public blink::WebWidget {
 public:
  MOCK_METHOD1(beginFrame, void(const blink::WebBeginFrameArgs& args));
};

class TestRenderWidget : public RenderWidget {
 public:
  TestRenderWidget()
      : RenderWidget(blink::WebPopupTypeNone,
                     blink::WebScreenInfo(),
                     true,
                     false,
                     false) {
    webwidget_ = &mock_webwidget_;
  }

  MockWebWidget mock_webwidget_;

 private:
  virtual ~TestRenderWidget() { webwidget_ = NULL; }

  DISALLOW_COPY_AND_ASSIGN(TestRenderWidget);
};

class RenderWidgetCompositorTest : public testing::Test {
 public:
  RenderWidgetCompositorTest()
      : render_widget_(make_scoped_refptr(new TestRenderWidget())),
        render_widget_compositor_(
            RenderWidgetCompositor::Create(render_widget_.get(), false)) {}
  virtual ~RenderWidgetCompositorTest() {}

 protected:
  MockRenderThread render_thread_;
  scoped_refptr<TestRenderWidget> render_widget_;
  scoped_ptr<RenderWidgetCompositor> render_widget_compositor_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetCompositorTest);
};

TEST_F(RenderWidgetCompositorTest, BeginMainFrame) {
  base::TimeTicks frame_time(base::TimeTicks() +
                             base::TimeDelta::FromSeconds(1));
  base::TimeTicks deadline(base::TimeTicks() + base::TimeDelta::FromSeconds(2));
  base::TimeDelta interval(base::TimeDelta::FromSeconds(3));
  cc::BeginFrameArgs args(
      cc::BeginFrameArgs::Create(frame_time, deadline, interval));

  EXPECT_CALL(render_widget_->mock_webwidget_,
              beginFrame(AllOf(
                  Field(&blink::WebBeginFrameArgs::lastFrameTimeMonotonic, 1),
                  Field(&blink::WebBeginFrameArgs::deadline, 2),
                  Field(&blink::WebBeginFrameArgs::interval, 3))));

  render_widget_compositor_->BeginMainFrame(args);
}

}  // namespace content
