// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ROUTER_UNITTEST_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ROUTER_UNITTEST_H_

#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/input/input_router_client.h"
#include "content/browser/renderer_host/input/mock_input_ack_handler.h"
#include "content/browser/renderer_host/input/mock_input_router_client.h"
#include "content/browser/renderer_host/input/synthetic_web_input_event_builders.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

class InputRouter;
class MockInputRouterClient;

class InputRouterTest : public testing::Test {
 public:
  InputRouterTest();
  virtual ~InputRouterTest();

 protected:
  // Called on SetUp.
  virtual scoped_ptr<InputRouter> CreateInputRouter(RenderProcessHost* process,
                                                    InputRouterClient* client,
                                                    InputAckHandler* handler,
                                                    int routing_id) = 0;

  // testing::Test
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  void SendInputEventACK(blink::WebInputEvent::Type type,
                         InputEventAckState ack_result);
  void SimulateKeyboardEvent(blink::WebInputEvent::Type type,
                             bool is_shortcut);
  void SimulateWheelEvent(float dX, float dY, int modifiers, bool precise);
  void SimulateMouseMove(int x, int y, int modifiers);
  void SimulateWheelEventWithPhase(blink::WebMouseWheelEvent::Phase phase);
  void SimulateGestureEvent(const blink::WebGestureEvent& event);
  void SimulateGestureEvent(blink::WebInputEvent::Type type,
                            blink::WebGestureEvent::SourceDevice sourceDevice);
  void SimulateGestureScrollUpdateEvent(float dX, float dY, int modifiers);
  void SimulateGesturePinchUpdateEvent(float scale,
                                       float anchorX,
                                       float anchorY,
                                       int modifiers);
  void SimulateGestureFlingStartEvent(
      float velocityX,
      float velocityY,
      blink::WebGestureEvent::SourceDevice sourceDevice);
  void SimulateTouchEvent(int x, int y);
  void SetTouchTimestamp(base::TimeDelta timestamp);

  // Sends a touch event (irrespective of whether the page has a touch-event
  // handler or not).
  void SendTouchEvent();

  int PressTouchPoint(int x, int y);
  void MoveTouchPoint(int index, int x, int y);
  void ReleaseTouchPoint(int index);

  scoped_ptr<MockRenderProcessHost> process_;
  scoped_ptr<MockInputRouterClient> client_;
  scoped_ptr<MockInputAckHandler> ack_handler_;
  scoped_ptr<InputRouter> input_router_;

 private:
  base::MessageLoopForUI message_loop_;
  SyntheticWebTouchEvent touch_event_;

  scoped_ptr<TestBrowserContext> browser_context_;
};

}  // namespace content

#endif // CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_ROUTER_UNITTEST_H_
