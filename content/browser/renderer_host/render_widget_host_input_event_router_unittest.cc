// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_input_event_router.h"

#include "base/test/scoped_task_environment.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/dummy_render_widget_host_delegate.h"
#include "content/test/test_render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// Used as a target for the RenderWidgetHostInputEventRouter. We record what
// events were forwarded to us in order to verify that the events are being
// routed correctly.
class MockRenderWidgetHostView : public TestRenderWidgetHostView {
 public:
  MockRenderWidgetHostView(RenderWidgetHost* rwh)
      : TestRenderWidgetHostView(rwh),
        last_gesture_seen_(blink::WebInputEvent::kUndefined) {}
  ~MockRenderWidgetHostView() override {}

  void ProcessGestureEvent(const blink::WebGestureEvent& event,
                           const ui::LatencyInfo&) override {
    last_gesture_seen_ = event.GetType();
  }

  blink::WebInputEvent::Type last_gesture_seen() { return last_gesture_seen_; }

  void Reset() { last_gesture_seen_ = blink::WebInputEvent::kUndefined; }

 private:
  blink::WebInputEvent::Type last_gesture_seen_;
};

// The RenderWidgetHostInputEventRouter uses the root RWHV for hittesting, so
// here we stub out the hittesting logic so we can control which RWHV will be
// the result of a hittest by the RWHIER.
class MockRootRenderWidgetHostView : public MockRenderWidgetHostView {
 public:
  MockRootRenderWidgetHostView(
      RenderWidgetHost* rwh,
      std::map<MockRenderWidgetHostView*, viz::FrameSinkId>& frame_sink_id_map)
      : MockRenderWidgetHostView(rwh), frame_sink_id_map_(frame_sink_id_map) {}
  ~MockRootRenderWidgetHostView() override {}

  viz::FrameSinkId FrameSinkIdAtPoint(viz::SurfaceHittestDelegate*,
                                      const gfx::Point&,
                                      gfx::Point*) override {
    return frame_sink_id_map_[current_hittest_result_];
  }

  void SetHittestResult(MockRenderWidgetHostView* view) {
    current_hittest_result_ = view;
  }

 private:
  std::map<MockRenderWidgetHostView*, viz::FrameSinkId>& frame_sink_id_map_;
  MockRenderWidgetHostView* current_hittest_result_;
};

}  // namespace

class RenderWidgetHostInputEventRouterTest : public testing::Test {
 public:
  RenderWidgetHostInputEventRouterTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}

 protected:
  // testing::Test:
  void SetUp() override {
    browser_context_ = base::MakeUnique<TestBrowserContext>();
    process_host1_ =
        base::MakeUnique<MockRenderProcessHost>(browser_context_.get());
    process_host2_ =
        base::MakeUnique<MockRenderProcessHost>(browser_context_.get());
    widget_host1_ = base::MakeUnique<RenderWidgetHostImpl>(
        &delegate_, process_host1_.get(), process_host1_->GetNextRoutingID(),
        false);
    widget_host2_ = base::MakeUnique<RenderWidgetHostImpl>(
        &delegate_, process_host2_.get(), process_host2_->GetNextRoutingID(),
        false);

    view_root_ = base::MakeUnique<MockRootRenderWidgetHostView>(
        widget_host1_.get(), frame_sink_id_map_);
    view_other_ =
        base::MakeUnique<MockRenderWidgetHostView>(widget_host2_.get());

    // Set up the RWHIER's FrameSinkId to RWHV map so that we can control the
    // result of RWHIER's hittesting.
    frame_sink_id_map_ = {{view_root_.get(), viz::FrameSinkId(1, 1)},
                          {view_other_.get(), viz::FrameSinkId(2, 2)}};
    rwhier_.AddFrameSinkIdOwner(frame_sink_id_map_[view_root_.get()],
                                view_root_.get());
    rwhier_.AddFrameSinkIdOwner(frame_sink_id_map_[view_other_.get()],
                                view_other_.get());
  }

  void TearDown() override { base::RunLoop().RunUntilIdle(); }

  RenderWidgetHostViewBase* touch_target() {
    return rwhier_.touch_target_.target;
  }
  RenderWidgetHostViewBase* touchscreen_gesture_target() {
    return rwhier_.touchscreen_gesture_target_.target;
  }

  // Needed by RenderWidgetHostImpl constructor.
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  DummyRenderWidgetHostDelegate delegate_;
  std::unique_ptr<BrowserContext> browser_context_;
  std::unique_ptr<MockRenderProcessHost> process_host1_;
  std::unique_ptr<MockRenderProcessHost> process_host2_;
  std::unique_ptr<RenderWidgetHostImpl> widget_host1_;
  std::unique_ptr<RenderWidgetHostImpl> widget_host2_;

  std::unique_ptr<MockRootRenderWidgetHostView> view_root_;
  std::unique_ptr<MockRenderWidgetHostView> view_other_;

  std::map<MockRenderWidgetHostView*, viz::FrameSinkId> frame_sink_id_map_;

  RenderWidgetHostInputEventRouter rwhier_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostInputEventRouterTest);
};

// Make sure that when a touch scroll crosses out of the area for a
// RenderWidgetHostView, the RenderWidgetHostInputEventRouter continues to
// route gesture events to the same RWHV until the end of the gesture.
// See crbug.com/739831
TEST_F(RenderWidgetHostInputEventRouterTest,
       DoNotChangeTargetViewDuringTouchScrollGesture) {
  // Simulate the touch and gesture events produced from scrolling on a
  // touchscreen.

  // We start the touch in the area for |view_other_|.
  view_root_->SetHittestResult(view_other_.get());

  blink::WebTouchEvent touch_event(blink::WebInputEvent::kTouchStart,
                                   blink::WebInputEvent::kNoModifiers,
                                   blink::WebInputEvent::kTimeStampForTesting);
  touch_event.touches_length = 1;
  touch_event.touches[0].state = blink::WebTouchPoint::kStatePressed;
  touch_event.unique_touch_event_id = 1;

  rwhier_.RouteTouchEvent(view_root_.get(), &touch_event,
                          ui::LatencyInfo(ui::SourceEventType::TOUCH));
  EXPECT_EQ(view_other_.get(), touch_target());

  blink::WebGestureEvent gesture_event(
      blink::WebInputEvent::kGestureTapDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::kTimeStampForTesting);
  gesture_event.source_device = blink::kWebGestureDeviceTouchscreen;
  gesture_event.unique_touch_event_id = touch_event.unique_touch_event_id;

  rwhier_.RouteGestureEvent(view_root_.get(), &gesture_event,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));
  EXPECT_EQ(view_other_.get(), touchscreen_gesture_target());
  EXPECT_EQ(blink::WebInputEvent::kGestureTapDown,
            view_other_->last_gesture_seen());
  EXPECT_NE(blink::WebInputEvent::kGestureTapDown,
            view_root_->last_gesture_seen());

  touch_event.SetType(blink::WebInputEvent::kTouchMove);
  touch_event.touches[0].state = blink::WebTouchPoint::kStateMoved;
  touch_event.unique_touch_event_id += 1;
  rwhier_.RouteTouchEvent(view_root_.get(), &touch_event,
                          ui::LatencyInfo(ui::SourceEventType::TOUCH));

  gesture_event.SetType(blink::WebInputEvent::kGestureTapCancel);
  gesture_event.unique_touch_event_id = touch_event.unique_touch_event_id;
  rwhier_.RouteGestureEvent(view_root_.get(), &gesture_event,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));

  gesture_event.SetType(blink::WebInputEvent::kGestureScrollBegin);
  rwhier_.RouteGestureEvent(view_root_.get(), &gesture_event,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));

  gesture_event.SetType(blink::WebInputEvent::kGestureScrollUpdate);
  rwhier_.RouteGestureEvent(view_root_.get(), &gesture_event,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));

  touch_event.unique_touch_event_id += 1;
  rwhier_.RouteTouchEvent(view_root_.get(), &touch_event,
                          ui::LatencyInfo(ui::SourceEventType::TOUCH));
  gesture_event.unique_touch_event_id = touch_event.unique_touch_event_id;
  rwhier_.RouteGestureEvent(view_root_.get(), &gesture_event,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));

  // Now the touch moves out of |view_other_| and into |view_root_|, but
  // |view_other_| should continue to be the target for gesture events.
  view_root_->SetHittestResult(view_root_.get());
  view_root_->Reset();
  view_other_->Reset();

  touch_event.unique_touch_event_id += 1;
  rwhier_.RouteTouchEvent(view_root_.get(), &touch_event,
                          ui::LatencyInfo(ui::SourceEventType::TOUCH));
  gesture_event.unique_touch_event_id = touch_event.unique_touch_event_id;
  rwhier_.RouteGestureEvent(view_root_.get(), &gesture_event,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));

  EXPECT_EQ(view_other_.get(), touch_target());
  EXPECT_EQ(view_other_.get(), touchscreen_gesture_target());
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollUpdate,
            view_other_->last_gesture_seen());
  EXPECT_NE(blink::WebInputEvent::kGestureScrollUpdate,
            view_root_->last_gesture_seen());

  touch_event.SetType(blink::WebInputEvent::kTouchEnd);
  touch_event.touches[0].state = blink::WebTouchPoint::kStateReleased;
  touch_event.unique_touch_event_id += 1;
  rwhier_.RouteTouchEvent(view_root_.get(), &touch_event,
                          ui::LatencyInfo(ui::SourceEventType::TOUCH));

  gesture_event.SetType(blink::WebInputEvent::kGestureScrollEnd);
  gesture_event.unique_touch_event_id = touch_event.unique_touch_event_id;
  rwhier_.RouteGestureEvent(view_root_.get(), &gesture_event,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));

  EXPECT_EQ(blink::WebInputEvent::kGestureScrollEnd,
            view_other_->last_gesture_seen());
  EXPECT_NE(blink::WebInputEvent::kGestureScrollEnd,
            view_root_->last_gesture_seen());
}

}  // namespace content
