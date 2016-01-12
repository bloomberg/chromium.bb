// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_widget.h"

#include <vector>

#include "base/macros.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/input_messages.h"
#include "content/public/test/mock_render_thread.h"
#include "content/test/fake_compositor_dependencies.h"
#include "content/test/mock_render_process.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

class InteractiveRenderWidget : public RenderWidget {
 public:
  explicit InteractiveRenderWidget(CompositorDependencies* compositor_deps)
      : RenderWidget(compositor_deps,
                     blink::WebPopupTypeNone,
                     blink::WebScreenInfo(),
                     false,
                     false,
                     false),
        always_overscroll_(false) {
    // A RenderWidget is not fully initialized until it has a routing ID.
    SetRoutingID(++next_routing_id_);
  }

  void SetTouchRegion(const std::vector<gfx::Rect>& rects) {
    rects_ = rects;
  }

  void SendInputEvent(const blink::WebInputEvent& event) {
    OnHandleInputEvent(&event, ui::LatencyInfo());
  }

  void set_always_overscroll(bool overscroll) {
    always_overscroll_ = overscroll;
  }

  IPC::TestSink* sink() { return &sink_; }

 protected:
  ~InteractiveRenderWidget() override {}

  // Overridden from RenderWidget:
  bool HasTouchEventHandlersAt(const gfx::Point& point) const override {
    for (std::vector<gfx::Rect>::const_iterator iter = rects_.begin();
         iter != rects_.end(); ++iter) {
      if ((*iter).Contains(point))
        return true;
    }
    return false;
  }

  bool WillHandleGestureEvent(const blink::WebGestureEvent& event) override {
    if (always_overscroll_ &&
        event.type == blink::WebInputEvent::GestureScrollUpdate) {
      didOverscroll(blink::WebFloatSize(event.data.scrollUpdate.deltaX,
                                        event.data.scrollUpdate.deltaY),
                    blink::WebFloatSize(event.data.scrollUpdate.deltaX,
                                        event.data.scrollUpdate.deltaY),
                    blink::WebFloatPoint(event.x, event.y),
                    blink::WebFloatSize(event.data.scrollUpdate.velocityX,
                                        event.data.scrollUpdate.velocityY));
      return true;
    }

    return false;
  }

  bool Send(IPC::Message* msg) override {
    sink_.OnMessageReceived(*msg);
    delete msg;
    return true;
  }

 private:
  std::vector<gfx::Rect> rects_;
  IPC::TestSink sink_;
  bool always_overscroll_;
  static int next_routing_id_;

  DISALLOW_COPY_AND_ASSIGN(InteractiveRenderWidget);
};

int InteractiveRenderWidget::next_routing_id_ = 0;

class RenderWidgetUnittest : public testing::Test {
 public:
  RenderWidgetUnittest()
      : widget_(new InteractiveRenderWidget(&compositor_deps_)) {}
  ~RenderWidgetUnittest() override {}

  InteractiveRenderWidget* widget() const { return widget_.get(); }

 private:
  MockRenderProcess render_process_;
  MockRenderThread render_thread_;
  FakeCompositorDependencies compositor_deps_;
  scoped_refptr<InteractiveRenderWidget> widget_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetUnittest);
};

TEST_F(RenderWidgetUnittest, TouchHitTestSinglePoint) {
  SyntheticWebTouchEvent touch;
  touch.PressPoint(10, 10);

  widget()->SendInputEvent(touch);
  ASSERT_EQ(1u, widget()->sink()->message_count());

  // Since there's currently no touch-event handling region, the response should
  // be 'no consumer exists'.
  const IPC::Message* message = widget()->sink()->GetMessageAt(0);
  EXPECT_EQ(InputHostMsg_HandleInputEvent_ACK::ID, message->type());
  InputHostMsg_HandleInputEvent_ACK::Param params;
  InputHostMsg_HandleInputEvent_ACK::Read(message, &params);
  InputEventAckState ack_state = base::get<0>(params).state;
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS, ack_state);
  widget()->sink()->ClearMessages();

  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect(0, 0, 20, 20));
  rects.push_back(gfx::Rect(25, 0, 10, 10));
  widget()->SetTouchRegion(rects);

  widget()->SendInputEvent(touch);
  ASSERT_EQ(1u, widget()->sink()->message_count());
  message = widget()->sink()->GetMessageAt(0);
  EXPECT_EQ(InputHostMsg_HandleInputEvent_ACK::ID, message->type());
  InputHostMsg_HandleInputEvent_ACK::Read(message, &params);
  ack_state = base::get<0>(params).state;
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, ack_state);
  widget()->sink()->ClearMessages();
}

TEST_F(RenderWidgetUnittest, TouchHitTestMultiplePoints) {
  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect(0, 0, 20, 20));
  rects.push_back(gfx::Rect(25, 0, 10, 10));
  widget()->SetTouchRegion(rects);

  SyntheticWebTouchEvent touch;
  touch.PressPoint(25, 25);

  widget()->SendInputEvent(touch);
  ASSERT_EQ(1u, widget()->sink()->message_count());

  // Since there's currently no touch-event handling region, the response should
  // be 'no consumer exists'.
  const IPC::Message* message = widget()->sink()->GetMessageAt(0);
  EXPECT_EQ(InputHostMsg_HandleInputEvent_ACK::ID, message->type());
  InputHostMsg_HandleInputEvent_ACK::Param params;
  InputHostMsg_HandleInputEvent_ACK::Read(message, &params);
  InputEventAckState ack_state = base::get<0>(params).state;
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS, ack_state);
  widget()->sink()->ClearMessages();

  // Press a second touch point. This time, on a touch-handling region.
  touch.PressPoint(10, 10);
  widget()->SendInputEvent(touch);
  ASSERT_EQ(1u, widget()->sink()->message_count());
  message = widget()->sink()->GetMessageAt(0);
  EXPECT_EQ(InputHostMsg_HandleInputEvent_ACK::ID, message->type());
  InputHostMsg_HandleInputEvent_ACK::Read(message, &params);
  ack_state = base::get<0>(params).state;
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, ack_state);
  widget()->sink()->ClearMessages();
}

TEST_F(RenderWidgetUnittest, EventOverscroll) {
  widget()->set_always_overscroll(true);

  blink::WebGestureEvent scroll;
  scroll.type = blink::WebInputEvent::GestureScrollUpdate;
  scroll.x = -10;
  scroll.data.scrollUpdate.deltaY = 10;
  widget()->SendInputEvent(scroll);

  // Overscroll notifications received while handling an input event should
  // be bundled with the event ack IPC.
  ASSERT_EQ(1u, widget()->sink()->message_count());
  const IPC::Message* message = widget()->sink()->GetMessageAt(0);
  ASSERT_EQ(InputHostMsg_HandleInputEvent_ACK::ID, message->type());
  InputHostMsg_HandleInputEvent_ACK::Param params;
  InputHostMsg_HandleInputEvent_ACK::Read(message, &params);
  const InputEventAck& ack = base::get<0>(params);
  ASSERT_EQ(ack.type, scroll.type);
  ASSERT_TRUE(ack.overscroll);
  EXPECT_EQ(gfx::Vector2dF(0, 10), ack.overscroll->accumulated_overscroll);
  EXPECT_EQ(gfx::Vector2dF(0, 10), ack.overscroll->latest_overscroll_delta);
  EXPECT_EQ(gfx::Vector2dF(), ack.overscroll->current_fling_velocity);
  EXPECT_EQ(gfx::PointF(-10, 0), ack.overscroll->causal_event_viewport_point);
  widget()->sink()->ClearMessages();
}

TEST_F(RenderWidgetUnittest, FlingOverscroll) {
  // Overscroll notifications received outside of handling an input event should
  // be sent as a separate IPC.
  widget()->didOverscroll(blink::WebFloatSize(10, 5), blink::WebFloatSize(5, 5),
                          blink::WebFloatPoint(1, 1),
                          blink::WebFloatSize(10, 5));
  ASSERT_EQ(1u, widget()->sink()->message_count());
  const IPC::Message* message = widget()->sink()->GetMessageAt(0);
  ASSERT_EQ(InputHostMsg_DidOverscroll::ID, message->type());
  InputHostMsg_DidOverscroll::Param params;
  InputHostMsg_DidOverscroll::Read(message, &params);
  const DidOverscrollParams& overscroll = base::get<0>(params);
  EXPECT_EQ(gfx::Vector2dF(10, 5), overscroll.latest_overscroll_delta);
  EXPECT_EQ(gfx::Vector2dF(5, 5), overscroll.accumulated_overscroll);
  EXPECT_EQ(gfx::PointF(1, 1), overscroll.causal_event_viewport_point);
  EXPECT_EQ(gfx::Vector2dF(-10, -5), overscroll.current_fling_velocity);
  widget()->sink()->ClearMessages();
}

}  // namespace content
