// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/renderer_host/input/gesture_event_filter.h"
#include "content/browser/renderer_host/input/immediate_input_router.h"
#include "content/browser/renderer_host/input/input_router_client.h"
#include "content/common/content_constants_internal.h"
#include "content/common/edit_command.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/keycodes/keyboard_codes.h"

#if defined(OS_WIN) || defined(USE_AURA)
#include "content/browser/renderer_host/ui_events_helper.h"
#include "ui/base/events/event.h"
#endif

using base::TimeDelta;
using WebKit::WebGestureEvent;
using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;
using WebKit::WebTouchEvent;
using WebKit::WebTouchPoint;

namespace content {

namespace {

const WebInputEvent* GetInputEventFromMessage(const IPC::Message& message) {
  PickleIterator iter(message);
  const char* data;
  int data_length;
  if (!message.ReadData(&iter, &data, &data_length))
    return NULL;
  return reinterpret_cast<const WebInputEvent*>(data);
}

bool GetIsShortcutFromHandleInputEventMessage(const IPC::Message* msg) {
  InputMsg_HandleInputEvent::Schema::Param param;
  InputMsg_HandleInputEvent::Read(msg, &param);
  return param.c;
}

template<typename MSG_T, typename ARG_T1>
void ExpectIPCMessageWithArg1(const IPC::Message* msg, const ARG_T1& arg1) {
  ASSERT_EQ(MSG_T::ID, msg->type());
  typename MSG_T::Schema::Param param;
  ASSERT_TRUE(MSG_T::Read(msg, &param));
  EXPECT_EQ(arg1, param.a);
}

template<typename MSG_T, typename ARG_T1, typename ARG_T2>
void ExpectIPCMessageWithArg2(const IPC::Message* msg,
                              const ARG_T1& arg1,
                              const ARG_T2& arg2) {
  ASSERT_EQ(MSG_T::ID, msg->type());
  typename MSG_T::Schema::Param param;
  ASSERT_TRUE(MSG_T::Read(msg, &param));
  EXPECT_EQ(arg1, param.a);
  EXPECT_EQ(arg2, param.b);
}

#if defined(OS_WIN) || defined(USE_AURA)
bool TouchEventsAreEquivalent(const ui::TouchEvent& first,
                              const ui::TouchEvent& second) {
  if (first.type() != second.type())
    return false;
  if (first.location() != second.location())
    return false;
  if (first.touch_id() != second.touch_id())
    return false;
  if (second.time_stamp().InSeconds() != first.time_stamp().InSeconds())
    return false;
  return true;
}

bool EventListIsSubset(const ScopedVector<ui::TouchEvent>& subset,
                       const ScopedVector<ui::TouchEvent>& set) {
  if (subset.size() > set.size())
    return false;
  for (size_t i = 0; i < subset.size(); ++i) {
    const ui::TouchEvent* first = subset[i];
    const ui::TouchEvent* second = set[i];
    bool equivalent = TouchEventsAreEquivalent(*first, *second);
    if (!equivalent)
      return false;
  }

  return true;
}
#endif  // defined(OS_WIN) || defined(USE_AURA)

}  // namespace

class MockInputRouterClient : public InputRouterClient {
 public:
  MockInputRouterClient()
      : input_router_(NULL),
        in_flight_event_count_(0),
        has_touch_handler_(false),
        ack_count_(0),
        unexpected_event_ack_called_(false),
        ack_state_(INPUT_EVENT_ACK_STATE_UNKNOWN),
        filter_state_(INPUT_EVENT_ACK_STATE_NOT_CONSUMED),
        is_shortcut_(false),
        allow_send_key_event_(true),
        send_called_(false),
        send_immediately_called_(false) {
  }
  virtual ~MockInputRouterClient() {
  }

  // InputRouterClient
  virtual InputEventAckState FilterInputEvent(
      const WebInputEvent& input_event,
      const ui::LatencyInfo& latency_info) OVERRIDE {
    return filter_state_;
  }

  // Called each time a WebInputEvent IPC is sent.
  virtual void IncrementInFlightEventCount() OVERRIDE {
    ++in_flight_event_count_;
  }

  // Called each time a WebInputEvent ACK IPC is received.
  virtual void DecrementInFlightEventCount() OVERRIDE {
    --in_flight_event_count_;
  }

  // Called when the renderer notifies that it has touch event handlers.
  virtual void OnHasTouchEventHandlers(bool has_handlers) OVERRIDE {
    has_touch_handler_ = has_handlers;
  }

  virtual bool OnSendKeyboardEvent(
      const NativeWebKeyboardEvent& key_event,
      const ui::LatencyInfo& latency_info,
      bool* is_shortcut) OVERRIDE {
    send_called_ = true;
    sent_key_event_ = key_event;
    *is_shortcut = is_shortcut_;

    return allow_send_key_event_;
  }

  virtual bool OnSendWheelEvent(
      const MouseWheelEventWithLatencyInfo& wheel_event) OVERRIDE {
    send_called_ = true;
    sent_wheel_event_ = wheel_event;

    return true;
  }

  virtual bool OnSendMouseEvent(
      const MouseEventWithLatencyInfo& mouse_event) OVERRIDE {
    send_called_ = true;
    sent_mouse_event_ = mouse_event;

    return true;
  }

  virtual bool OnSendTouchEvent(
      const TouchEventWithLatencyInfo& touch_event) OVERRIDE {
    send_called_ = true;
    sent_touch_event_ = touch_event;

    return true;
  }

  virtual bool OnSendGestureEvent(
      const GestureEventWithLatencyInfo& gesture_event) OVERRIDE {
    send_called_ = true;
    sent_gesture_event_ = gesture_event;

    return input_router_->ShouldForwardGestureEvent(gesture_event);
  }

  virtual bool OnSendMouseEventImmediately(
      const MouseEventWithLatencyInfo& mouse_event) OVERRIDE {
    send_immediately_called_ = true;
    immediately_sent_mouse_event_ = mouse_event;

    return true;
  }

  virtual bool OnSendTouchEventImmediately(
      const TouchEventWithLatencyInfo& touch_event) OVERRIDE {
    send_immediately_called_ = true;
    immediately_sent_touch_event_ = touch_event;

    return true;
  }

  virtual bool OnSendGestureEventImmediately(
      const GestureEventWithLatencyInfo& gesture_event) OVERRIDE {
    send_immediately_called_ = true;
    immediately_sent_gesture_event_ = gesture_event;

    return true;
  }

  // Called upon event ack receipt from the renderer.
  virtual void OnKeyboardEventAck(const NativeWebKeyboardEvent& event,
                                  InputEventAckState ack_result) OVERRIDE {
    VLOG(1) << __FUNCTION__ << " called!";
    acked_key_event_ = event;
    RecordAckCalled(ack_result);
  }
  virtual void OnWheelEventAck(const WebMouseWheelEvent& event,
                               InputEventAckState ack_result) OVERRIDE {
    VLOG(1) << __FUNCTION__ << " called!";
    acked_wheel_event_ = event;
    RecordAckCalled(ack_result);
  }
  virtual void OnTouchEventAck(const TouchEventWithLatencyInfo& event,
                               InputEventAckState ack_result) OVERRIDE {
    VLOG(1) << __FUNCTION__ << " called!";
    acked_touch_event_ = event;
    RecordAckCalled(ack_result);
  }
  virtual void OnGestureEventAck(const WebGestureEvent& event,
                                 InputEventAckState ack_result) OVERRIDE {
    VLOG(1) << __FUNCTION__ << " called!";
    RecordAckCalled(ack_result);
  }
  virtual void OnUnexpectedEventAck(bool bad_message) OVERRIDE {
    VLOG(1) << __FUNCTION__ << " called!";
    unexpected_event_ack_called_ = true;
  }

  void ExpectSendCalled(bool called) {
    EXPECT_EQ(called, send_called_);
    send_called_ = false;
  }
  void ExpectSendImmediatelyCalled(bool called) {
    EXPECT_EQ(called, send_immediately_called_);
    send_immediately_called_ = false;
  }
  void ExpectAckCalled(int times) {
    EXPECT_EQ(times, ack_count_);
    ack_count_ = 0;
  }

  void set_input_router(InputRouter* input_router) {
    input_router_ = input_router;
  }
  bool has_touch_handler() const { return has_touch_handler_; }
  InputEventAckState ack_state() const { return ack_state_; }
  void set_filter_state(InputEventAckState filter_state) {
    filter_state_ = filter_state;
  }
  bool unexpected_event_ack_called() const {
    return unexpected_event_ack_called_;
  }
  const NativeWebKeyboardEvent& acked_keyboard_event() {
    return acked_key_event_;
  }
  const WebMouseWheelEvent& acked_wheel_event() {
    return acked_wheel_event_;
  }
  const TouchEventWithLatencyInfo& acked_touch_event() {
    return acked_touch_event_;
  }
  void set_is_shortcut(bool is_shortcut) {
    is_shortcut_ = is_shortcut;
  }
  void set_allow_send_key_event(bool allow) {
    allow_send_key_event_ = allow;
  }
  const NativeWebKeyboardEvent& sent_key_event() {
    return sent_key_event_;
  }
  const MouseWheelEventWithLatencyInfo& sent_wheel_event() {
    return sent_wheel_event_;
  }
  const MouseEventWithLatencyInfo& sent_mouse_event() {
    return sent_mouse_event_;
  }
  const GestureEventWithLatencyInfo& sent_gesture_event() {
    return sent_gesture_event_;
  }
  const MouseEventWithLatencyInfo& immediately_sent_mouse_event() {
    return immediately_sent_mouse_event_;
  }
  const TouchEventWithLatencyInfo& immediately_sent_touch_event() {
    return immediately_sent_touch_event_;
  }
  const GestureEventWithLatencyInfo& immediately_sent_gesture_event() {
    return immediately_sent_gesture_event_;
  }

 private:
  void RecordAckCalled(InputEventAckState ack_result) {
    ++ack_count_;
    ack_state_ = ack_result;
  }

  InputRouter* input_router_;
  int in_flight_event_count_;
  bool has_touch_handler_;

  int ack_count_;
  bool unexpected_event_ack_called_;
  InputEventAckState ack_state_;
  InputEventAckState filter_state_;
  NativeWebKeyboardEvent acked_key_event_;
  WebMouseWheelEvent acked_wheel_event_;
  TouchEventWithLatencyInfo acked_touch_event_;

  bool is_shortcut_;
  bool allow_send_key_event_;
  bool send_called_;
  NativeWebKeyboardEvent sent_key_event_;
  MouseWheelEventWithLatencyInfo sent_wheel_event_;
  MouseEventWithLatencyInfo sent_mouse_event_;
  TouchEventWithLatencyInfo sent_touch_event_;
  GestureEventWithLatencyInfo sent_gesture_event_;

  bool send_immediately_called_;
  MouseEventWithLatencyInfo immediately_sent_mouse_event_;
  TouchEventWithLatencyInfo immediately_sent_touch_event_;
  GestureEventWithLatencyInfo immediately_sent_gesture_event_;
};

class ImmediateInputRouterTest : public testing::Test {
 public:
  ImmediateInputRouterTest() {
  }
  virtual ~ImmediateInputRouterTest() {
  }

 protected:
  // testing::Test
  virtual void SetUp() {
    browser_context_.reset(new TestBrowserContext());
    process_.reset(new MockRenderProcessHost(browser_context_.get()));
    client_.reset(new MockInputRouterClient());
    input_router_.reset(new ImmediateInputRouter(
        process_.get(),
        client_.get(),
        MSG_ROUTING_NONE));
    client_->set_input_router(input_router_.get());
  }
  virtual void TearDown() {
    // Process all pending tasks to avoid leaks.
    base::MessageLoop::current()->RunUntilIdle();

    input_router_.reset();
    client_.reset();
    process_.reset();
    browser_context_.reset();
  }

  void SendInputEventACK(WebInputEvent::Type type,
                         InputEventAckState ack_result) {
    scoped_ptr<IPC::Message> response(
        new InputHostMsg_HandleInputEvent_ACK(0, type, ack_result,
                                              ui::LatencyInfo()));
    input_router_->OnMessageReceived(*response);
  }

  void SimulateKeyboardEvent(WebInputEvent::Type type) {
    NativeWebKeyboardEvent key_event;
    key_event.type = type;
    key_event.windowsKeyCode = ui::VKEY_L;  // non-null made up value.
    input_router_->SendKeyboardEvent(key_event, ui::LatencyInfo());
    client_->ExpectSendCalled(true);
    EXPECT_EQ(type, client_->sent_key_event().type);
    EXPECT_EQ(key_event.windowsKeyCode,
              client_->sent_key_event().windowsKeyCode);
  }

  void SimulateWheelEvent(float dX, float dY, int modifiers, bool precise) {
    WebMouseWheelEvent wheel_event;
    wheel_event.type = WebInputEvent::MouseWheel;
    wheel_event.deltaX = dX;
    wheel_event.deltaY = dY;
    wheel_event.modifiers = modifiers;
    wheel_event.hasPreciseScrollingDeltas = precise;
    input_router_->SendWheelEvent(
        MouseWheelEventWithLatencyInfo(wheel_event, ui::LatencyInfo()));
    client_->ExpectSendCalled(true);
    EXPECT_EQ(wheel_event.type, client_->sent_wheel_event().event.type);
    EXPECT_EQ(dX, client_->sent_wheel_event().event.deltaX);
  }

  void SimulateMouseMove(int x, int y, int modifiers) {
    WebMouseEvent mouse_event;
    mouse_event.type = WebInputEvent::MouseMove;
    mouse_event.x = mouse_event.windowX = x;
    mouse_event.y = mouse_event.windowY = y;
    mouse_event.modifiers = modifiers;
    input_router_->SendMouseEvent(
        MouseEventWithLatencyInfo(mouse_event, ui::LatencyInfo()));
    client_->ExpectSendCalled(true);
    client_->ExpectSendImmediatelyCalled(true);
    EXPECT_EQ(mouse_event.type, client_->sent_mouse_event().event.type);
    EXPECT_EQ(x, client_->sent_mouse_event().event.x);
    EXPECT_EQ(mouse_event.type,
              client_->immediately_sent_mouse_event().event.type);
    EXPECT_EQ(x, client_->immediately_sent_mouse_event().event.x);
  }

  void SimulateWheelEventWithPhase(WebMouseWheelEvent::Phase phase) {
    WebMouseWheelEvent wheel_event;
    wheel_event.type = WebInputEvent::MouseWheel;
    wheel_event.phase = phase;
    input_router_->SendWheelEvent(
        MouseWheelEventWithLatencyInfo(wheel_event, ui::LatencyInfo()));
    client_->ExpectSendCalled(true);
    EXPECT_EQ(wheel_event.type, client_->sent_wheel_event().event.type);
    EXPECT_EQ(phase, client_->sent_wheel_event().event.phase);
  }

  // Inject provided synthetic WebGestureEvent instance.
  void SimulateGestureEventCore(WebInputEvent::Type type,
                            WebGestureEvent::SourceDevice sourceDevice,
                            WebGestureEvent* gesture_event) {
    gesture_event->type = type;
    gesture_event->sourceDevice = sourceDevice;
    GestureEventWithLatencyInfo gesture_with_latency(
        *gesture_event, ui::LatencyInfo());
    input_router_->SendGestureEvent(gesture_with_latency);
    client_->ExpectSendCalled(true);
    EXPECT_EQ(type, client_->sent_gesture_event().event.type);
    EXPECT_EQ(sourceDevice, client_->sent_gesture_event().event.sourceDevice);
  }

  // Inject simple synthetic WebGestureEvent instances.
  void SimulateGestureEvent(WebInputEvent::Type type,
                            WebGestureEvent::SourceDevice sourceDevice) {
    WebGestureEvent gesture_event;
    SimulateGestureEventCore(type, sourceDevice, &gesture_event);
  }

  void SimulateGestureScrollUpdateEvent(float dX, float dY, int modifiers) {
    WebGestureEvent gesture_event;
    gesture_event.data.scrollUpdate.deltaX = dX;
    gesture_event.data.scrollUpdate.deltaY = dY;
    gesture_event.modifiers = modifiers;
    SimulateGestureEventCore(WebInputEvent::GestureScrollUpdate,
                             WebGestureEvent::Touchscreen, &gesture_event);
  }

  void SimulateGesturePinchUpdateEvent(float scale,
                                       float anchorX,
                                       float anchorY,
                                       int modifiers) {
    WebGestureEvent gesture_event;
    gesture_event.data.pinchUpdate.scale = scale;
    gesture_event.x = anchorX;
    gesture_event.y = anchorY;
    gesture_event.modifiers = modifiers;
    SimulateGestureEventCore(WebInputEvent::GesturePinchUpdate,
                             WebGestureEvent::Touchscreen, &gesture_event);
  }

  // Inject synthetic GestureFlingStart events.
  void SimulateGestureFlingStartEvent(
      float velocityX,
      float velocityY,
      WebGestureEvent::SourceDevice sourceDevice) {
    WebGestureEvent gesture_event;
    gesture_event.data.flingStart.velocityX = velocityX;
    gesture_event.data.flingStart.velocityY = velocityY;
    SimulateGestureEventCore(WebInputEvent::GestureFlingStart, sourceDevice,
                             &gesture_event);
  }

  // Set the timestamp for the touch-event.
  void SetTouchTimestamp(base::TimeDelta timestamp) {
    touch_event_.timeStampSeconds = timestamp.InSecondsF();
  }

  // Sends a touch event (irrespective of whether the page has a touch-event
  // handler or not).
  void SendTouchEvent() {
    input_router_->SendTouchEvent(
        TouchEventWithLatencyInfo(touch_event_, ui::LatencyInfo()));

    // Mark all the points as stationary. And remove the points that have been
    // released.
    int point = 0;
    for (unsigned int i = 0; i < touch_event_.touchesLength; ++i) {
      if (touch_event_.touches[i].state == WebTouchPoint::StateReleased)
        continue;

      touch_event_.touches[point] = touch_event_.touches[i];
      touch_event_.touches[point].state =
          WebTouchPoint::StateStationary;
      ++point;
    }
    touch_event_.touchesLength = point;
    touch_event_.type = WebInputEvent::Undefined;
  }

  int PressTouchPoint(int x, int y) {
    if (touch_event_.touchesLength == touch_event_.touchesLengthCap)
      return -1;
    WebTouchPoint& point =
        touch_event_.touches[touch_event_.touchesLength];
    point.id = touch_event_.touchesLength;
    point.position.x = point.screenPosition.x = x;
    point.position.y = point.screenPosition.y = y;
    point.state = WebTouchPoint::StatePressed;
    point.radiusX = point.radiusY = 1.f;
    ++touch_event_.touchesLength;
    touch_event_.type = WebInputEvent::TouchStart;
    return point.id;
  }

  void MoveTouchPoint(int index, int x, int y) {
    CHECK(index >= 0 && index < touch_event_.touchesLengthCap);
    WebTouchPoint& point = touch_event_.touches[index];
    point.position.x = point.screenPosition.x = x;
    point.position.y = point.screenPosition.y = y;
    touch_event_.touches[index].state = WebTouchPoint::StateMoved;
    touch_event_.type = WebInputEvent::TouchMove;
  }

  void ReleaseTouchPoint(int index) {
    CHECK(index >= 0 && index < touch_event_.touchesLengthCap);
    touch_event_.touches[index].state = WebTouchPoint::StateReleased;
    touch_event_.type = WebInputEvent::TouchEnd;
  }

  void set_debounce_interval_time_ms(int ms) {
    input_router_->gesture_event_filter()->debounce_interval_time_ms_ = ms;
  }

  void set_maximum_tap_gap_time_ms(int delay_ms) {
    input_router_->gesture_event_filter()->maximum_tap_gap_time_ms_ = delay_ms;
  }

  size_t TouchEventQueueSize() {
    return touch_event_queue()->GetQueueSize();
  }

  const WebTouchEvent& latest_event() const {
    return touch_event_queue()->GetLatestEvent().event;
  }

  TouchEventQueue* touch_event_queue() const {
    return input_router_->touch_event_queue();
  }

  unsigned GestureEventLastQueueEventSize() {
    return gesture_event_filter()->coalesced_gesture_events_.size();
  }

  WebGestureEvent GestureEventSecondFromLastQueueEvent() {
    return gesture_event_filter()->coalesced_gesture_events_.at(
      GestureEventLastQueueEventSize() - 2).event;
  }

  WebGestureEvent GestureEventLastQueueEvent() {
    return gesture_event_filter()->coalesced_gesture_events_.back().event;
  }

  unsigned GestureEventDebouncingQueueSize() {
    return gesture_event_filter()->debouncing_deferral_queue_.size();
  }

  WebGestureEvent GestureEventQueueEventAt(int i) {
    return gesture_event_filter()->coalesced_gesture_events_.at(i).event;
  }

  bool shouldDeferTapDownEvents() {
    return gesture_event_filter()->maximum_tap_gap_time_ms_ != 0;
  }

  bool ScrollingInProgress() {
    return gesture_event_filter()->scrolling_in_progress_;
  }

  bool FlingInProgress() {
    return gesture_event_filter()->fling_in_progress_;
  }

  bool WillIgnoreNextACK() {
    return gesture_event_filter()->ignore_next_ack_;
  }

  GestureEventFilter* gesture_event_filter() const {
    return input_router_->gesture_event_filter();
  }

  scoped_ptr<MockRenderProcessHost> process_;
  scoped_ptr<MockInputRouterClient> client_;
  scoped_ptr<ImmediateInputRouter> input_router_;

 private:
  base::MessageLoopForUI message_loop_;
  WebTouchEvent touch_event_;

  scoped_ptr<TestBrowserContext> browser_context_;
};

#if GTEST_HAS_PARAM_TEST
// This is for tests that are to be run for all source devices.
class ImmediateInputRouterWithSourceTest
    : public ImmediateInputRouterTest,
      public testing::WithParamInterface<WebGestureEvent::SourceDevice> {
};
#endif  // GTEST_HAS_PARAM_TEST

TEST_F(ImmediateInputRouterTest, CoalescesRangeSelection) {
  input_router_->SendInput(
      new InputMsg_SelectRange(0, gfx::Point(1, 2), gfx::Point(3, 4)));
  EXPECT_EQ(1u, process_->sink().message_count());
  ExpectIPCMessageWithArg2<InputMsg_SelectRange>(
      process_->sink().GetMessageAt(0),
      gfx::Point(1, 2),
      gfx::Point(3, 4));
  process_->sink().ClearMessages();

  // Send two more messages without acking.
  input_router_->SendInput(
      new InputMsg_SelectRange(0, gfx::Point(5, 6), gfx::Point(7, 8)));
  EXPECT_EQ(0u, process_->sink().message_count());

  input_router_->SendInput(
      new InputMsg_SelectRange(0, gfx::Point(9, 10), gfx::Point(11, 12)));
  EXPECT_EQ(0u, process_->sink().message_count());

  // Now ack the first message.
  {
    scoped_ptr<IPC::Message> response(new ViewHostMsg_SelectRange_ACK(0));
    input_router_->OnMessageReceived(*response);
  }

  // Verify that the two messages are coalesced into one message.
  EXPECT_EQ(1u, process_->sink().message_count());
  ExpectIPCMessageWithArg2<InputMsg_SelectRange>(
      process_->sink().GetMessageAt(0),
      gfx::Point(9, 10),
      gfx::Point(11, 12));
  process_->sink().ClearMessages();

  // Acking the coalesced msg should not send any more msg.
  {
    scoped_ptr<IPC::Message> response(new ViewHostMsg_SelectRange_ACK(0));
    input_router_->OnMessageReceived(*response);
  }
  EXPECT_EQ(0u, process_->sink().message_count());
}

TEST_F(ImmediateInputRouterTest, CoalescesCaretMove) {
  input_router_->SendInput(new InputMsg_MoveCaret(0, gfx::Point(1, 2)));
  EXPECT_EQ(1u, process_->sink().message_count());
  ExpectIPCMessageWithArg1<InputMsg_MoveCaret>(
      process_->sink().GetMessageAt(0), gfx::Point(1, 2));
  process_->sink().ClearMessages();

  // Send two more messages without acking.
  input_router_->SendInput(new InputMsg_MoveCaret(0, gfx::Point(5, 6)));
  EXPECT_EQ(0u, process_->sink().message_count());

  input_router_->SendInput(new InputMsg_MoveCaret(0, gfx::Point(9, 10)));
  EXPECT_EQ(0u, process_->sink().message_count());

  // Now ack the first message.
  {
    scoped_ptr<IPC::Message> response(new ViewHostMsg_MoveCaret_ACK(0));
    input_router_->OnMessageReceived(*response);
  }

  // Verify that the two messages are coalesced into one message.
  EXPECT_EQ(1u, process_->sink().message_count());
  ExpectIPCMessageWithArg1<InputMsg_MoveCaret>(
      process_->sink().GetMessageAt(0), gfx::Point(9, 10));
  process_->sink().ClearMessages();

  // Acking the coalesced msg should not send any more msg.
  {
    scoped_ptr<IPC::Message> response(new ViewHostMsg_MoveCaret_ACK(0));
    input_router_->OnMessageReceived(*response);
  }
  EXPECT_EQ(0u, process_->sink().message_count());
}

TEST_F(ImmediateInputRouterTest, HandledInputEvent) {
  client_->set_filter_state(INPUT_EVENT_ACK_STATE_CONSUMED);

  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);

  // Make sure no input event is sent to the renderer.
  EXPECT_EQ(0u, process_->sink().message_count());

  // OnKeyboardEventAck should be triggered without actual ack.
  client_->ExpectAckCalled(1);

  // As the event was acked already, keyboard event queue should be
  // empty.
  ASSERT_EQ(NULL, input_router_->GetLastKeyboardEvent());
}

TEST_F(ImmediateInputRouterTest, ClientCanceledKeyboardEvent) {
  client_->set_allow_send_key_event(false);

  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);

  // Make sure no input event is sent to the renderer.
  EXPECT_EQ(0u, process_->sink().message_count());
  client_->ExpectAckCalled(0);
}

TEST_F(ImmediateInputRouterTest, ShortcutKeyboardEvent) {
  client_->set_is_shortcut(true);
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);
  EXPECT_TRUE(GetIsShortcutFromHandleInputEventMessage(
      process_->sink().GetMessageAt(0)));

  process_->sink().ClearMessages();

  client_->set_is_shortcut(false);
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);
  EXPECT_FALSE(GetIsShortcutFromHandleInputEventMessage(
      process_->sink().GetMessageAt(0)));
}

TEST_F(ImmediateInputRouterTest, NoncorrespondingKeyEvents) {
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);

  SendInputEventACK(WebInputEvent::KeyUp,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(client_->unexpected_event_ack_called());
}

// Tests ported from RenderWidgetHostTest --------------------------------------

TEST_F(ImmediateInputRouterTest, HandleKeyEventsWeSent) {
  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);
  ASSERT_TRUE(input_router_->GetLastKeyboardEvent());
  EXPECT_EQ(WebInputEvent::RawKeyDown,
            input_router_->GetLastKeyboardEvent()->type);

  // Make sure we sent the input event to the renderer.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::RawKeyDown,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  client_->ExpectAckCalled(1);
  EXPECT_EQ(WebInputEvent::RawKeyDown, client_->acked_keyboard_event().type);
}

TEST_F(ImmediateInputRouterTest, IgnoreKeyEventsWeDidntSend) {
  // Send a simulated, unrequested key response. We should ignore this.
  SendInputEventACK(WebInputEvent::RawKeyDown,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  client_->ExpectAckCalled(0);
}

TEST_F(ImmediateInputRouterTest, CoalescesWheelEvents) {
  // Simulate wheel events.
  SimulateWheelEvent(0, -5, 0, false);  // sent directly
  SimulateWheelEvent(0, -10, 0, false);  // enqueued
  SimulateWheelEvent(8, -6, 0, false);  // coalesced into previous event
  SimulateWheelEvent(9, -7, 1, false);  // enqueued, different modifiers

  // Check that only the first event was sent.
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Check that the ACK sends the second message via ImmediateInputForwarder
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  // The coalesced events can queue up a delayed ack
  // so that additional input events can be processed before
  // we turn off coalescing.
  base::MessageLoop::current()->RunUntilIdle();
  client_->ExpectAckCalled(1);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
          InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // One more time.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  client_->ExpectAckCalled(1);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // After the final ack, the queue should be empty.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  client_->ExpectAckCalled(1);
  EXPECT_EQ(0U, process_->sink().message_count());

  // FIXME(kouhei): Below is testing gesture event filter. Maybe separate test?
  {
    WebGestureEvent gesture_event;
    gesture_event.type = WebInputEvent::GestureFlingStart;
    gesture_event.sourceDevice = WebGestureEvent::Touchpad;
    gesture_event.data.flingStart.velocityX = 0.f;
    gesture_event.data.flingStart.velocityY = 0.f;
    EXPECT_FALSE(input_router_->ShouldForwardGestureEvent(
        GestureEventWithLatencyInfo(gesture_event, ui::LatencyInfo())));
  }
}

TEST_F(ImmediateInputRouterTest,
       CoalescesWheelEventsQueuedPhaseEndIsNotDropped) {
  // Send an initial gesture begin and ACK it.
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchpad);
  EXPECT_EQ(1U, process_->sink().message_count());
  SendInputEventACK(WebInputEvent::GestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();

  // Send a wheel event, should get sent directly.
  SimulateWheelEvent(0, -5, 0, false);
  EXPECT_EQ(2U, process_->sink().message_count());

  // Send a wheel phase end event before an ACK is received for the previous
  // wheel event, which should get queued.
  SimulateWheelEventWithPhase(WebMouseWheelEvent::PhaseEnded);
  EXPECT_EQ(2U, process_->sink().message_count());

  // A gesture event should now result in the queued phase ended event being
  // transmitted before it.
  SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                       WebGestureEvent::Touchpad);
  ASSERT_EQ(4U, process_->sink().message_count());

  // Verify the events that were sent.
  const WebInputEvent* input_event =
      GetInputEventFromMessage(*process_->sink().GetMessageAt(2));
  ASSERT_EQ(WebInputEvent::MouseWheel, input_event->type);
  const WebMouseWheelEvent* wheel_event =
      static_cast<const WebMouseWheelEvent*>(input_event);
  ASSERT_EQ(WebMouseWheelEvent::PhaseEnded, wheel_event->phase);

  input_event = GetInputEventFromMessage(*process_->sink().GetMessageAt(3));
  EXPECT_EQ(WebInputEvent::GestureScrollEnd, input_event->type);
}

TEST_F(ImmediateInputRouterTest, CoalescesScrollGestureEvents) {
  // Turn off debounce handling for test isolation.
  set_debounce_interval_time_ms(0);

  // Test coalescing of only GestureScrollUpdate events.
  // Simulate gesture events.

  // Sent.
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchscreen);

  // Enqueued.
  SimulateGestureScrollUpdateEvent(8, -5, 0);

  // Make sure that the queue contains what we think it should.
  WebGestureEvent merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(2U, GestureEventLastQueueEventSize());
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);

  // Coalesced.
  SimulateGestureScrollUpdateEvent(8, -6, 0);

  // Check that coalescing updated the correct values.
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(0, merged_event.modifiers);
  EXPECT_EQ(16, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-11, merged_event.data.scrollUpdate.deltaY);

  // Enqueued.
  SimulateGestureScrollUpdateEvent(8, -7, 1);

  // Check that we didn't wrongly coalesce.
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(1, merged_event.modifiers);

  // Different.
  SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                       WebGestureEvent::Touchscreen);

  // Check that only the first event was sent.
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
              InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Check that the ACK sends the second message.
  SendInputEventACK(WebInputEvent::GestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  client_->ExpectAckCalled(1);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
              InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Ack for queued coalesced event.
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  client_->ExpectAckCalled(1);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
              InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Ack for queued uncoalesced event.
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  client_->ExpectAckCalled(1);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
              InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // After the final ack, the queue should be empty.
  SendInputEventACK(WebInputEvent::GestureScrollEnd,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  client_->ExpectAckCalled(1);
  EXPECT_EQ(0U, process_->sink().message_count());
}

TEST_F(ImmediateInputRouterTest, CoalescesScrollAndPinchEvents) {
  // Turn off debounce handling for test isolation.
  set_debounce_interval_time_ms(0);

  // Test coalescing of only GestureScrollUpdate events.
  // Simulate gesture events.

  // Sent.
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchscreen);

  // Sent.
  SimulateGestureEvent(WebInputEvent::GesturePinchBegin,
                       WebGestureEvent::Touchscreen);

  // Enqueued.
  SimulateGestureScrollUpdateEvent(8, -4, 1);

  // Make sure that the queue contains what we think it should.
  WebGestureEvent merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(3U, GestureEventLastQueueEventSize());
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);

  // Coalesced without changing event order. Note anchor at (60, 60). Anchoring
  // from a poinht that is not the origin should still give us the wight scroll.
  SimulateGesturePinchUpdateEvent(1.5, 60, 60, 1);
  EXPECT_EQ(4U, GestureEventLastQueueEventSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(1.5, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(8, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-4, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);

  // Enqueued.
  SimulateGestureScrollUpdateEvent(6, -3, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(4U, GestureEventLastQueueEventSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(1.5, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(12, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-6, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);

  // Enqueued.
  SimulateGesturePinchUpdateEvent(2, 60, 60, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(4U, GestureEventLastQueueEventSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(3, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(12, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-6, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);

  // Enqueued.
  SimulateGesturePinchUpdateEvent(2, 60, 60, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(4U, GestureEventLastQueueEventSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(6, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(12, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-6, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);

  // Check that only the first event was sent.
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
              InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Check that the ACK sends the second message.
  SendInputEventACK(WebInputEvent::GestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
              InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Enqueued.
  SimulateGestureScrollUpdateEvent(6, -6, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(3U, GestureEventLastQueueEventSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(6, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(13, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-7, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);

  // At this point ACKs shouldn't be getting ignored.
  EXPECT_FALSE(WillIgnoreNextACK());

  // Check that the ACK sends both scroll and pinch updates.
  SendInputEventACK(WebInputEvent::GesturePinchBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(2U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetFirstMessageMatching(
              InputMsg_HandleInputEvent::ID));
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(
              InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // The next ACK should be getting ignored.
  EXPECT_TRUE(WillIgnoreNextACK());

  // Enqueued.
  SimulateGestureScrollUpdateEvent(1, -1, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(3U, GestureEventLastQueueEventSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(1, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-1, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(6, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);

  // Enqueued.
  SimulateGestureScrollUpdateEvent(2, -2, 1);

  // Coalescing scrolls should still work.
  EXPECT_EQ(3U, GestureEventLastQueueEventSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(3, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-3, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(6, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);

  // Enqueued.
  SimulateGesturePinchUpdateEvent(0.5, 60, 60, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(4U, GestureEventLastQueueEventSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(0.5, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(3, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-3, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);

  // Check that the ACK gets ignored.
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(0U, process_->sink().message_count());
  // The flag should have been flipped back to false.
  EXPECT_FALSE(WillIgnoreNextACK());

  // Enqueued.
  SimulateGestureScrollUpdateEvent(2, -2, 2);

  // Shouldn't coalesce with different modifiers.
  EXPECT_EQ(4U, GestureEventLastQueueEventSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(2, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-2, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(2, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(0.5, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);

  // Check that the ACK sends the next scroll pinch pair.
  SendInputEventACK(WebInputEvent::GesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(2U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetFirstMessageMatching(
              InputMsg_HandleInputEvent::ID));
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(
              InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Check that the ACK sends the second message.
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(0U, process_->sink().message_count());

  // Check that the ACK sends the second message.
  SendInputEventACK(WebInputEvent::GesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
              InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Check that the queue is empty after ACK and no messages get sent.
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());
}

#if GTEST_HAS_PARAM_TEST
TEST_P(ImmediateInputRouterWithSourceTest, GestureFlingCancelsFiltered) {
  WebGestureEvent::SourceDevice source_device = GetParam();

  // Turn off debounce handling for test isolation.
  set_debounce_interval_time_ms(0);
  // GFC without previous GFS is dropped.
  SimulateGestureEvent(WebInputEvent::GestureFlingCancel, source_device);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());

  // GFC after previous GFS is dispatched and acked.
  process_->sink().ClearMessages();
  SimulateGestureFlingStartEvent(0, -10, source_device);
  EXPECT_TRUE(FlingInProgress());
  SendInputEventACK(WebInputEvent::GestureFlingStart,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  client_->ExpectAckCalled(1);
  SimulateGestureEvent(WebInputEvent::GestureFlingCancel, source_device);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(2U, process_->sink().message_count());
  SendInputEventACK(WebInputEvent::GestureFlingCancel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  client_->ExpectAckCalled(1);
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());

  // GFC before previous GFS is acked.
  process_->sink().ClearMessages();
  SimulateGestureFlingStartEvent(0, -10, source_device);
  EXPECT_TRUE(FlingInProgress());
  SimulateGestureEvent(WebInputEvent::GestureFlingCancel, source_device);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(2U, GestureEventLastQueueEventSize());

  // Advance state realistically.
  SendInputEventACK(WebInputEvent::GestureFlingStart,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  SendInputEventACK(WebInputEvent::GestureFlingCancel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  client_->ExpectAckCalled(2);
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());

  // GFS is added to the queue if another event is pending
  process_->sink().ClearMessages();
  SimulateGestureScrollUpdateEvent(8, -7, 0);
  SimulateGestureFlingStartEvent(0, -10, source_device);
  EXPECT_EQ(2U, GestureEventLastQueueEventSize());
  EXPECT_EQ(1U, process_->sink().message_count());
  WebGestureEvent merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureFlingStart, merged_event.type);
  EXPECT_TRUE(FlingInProgress());
  EXPECT_EQ(2U, GestureEventLastQueueEventSize());

  // GFS in queue means that a GFC is added to the queue
  SimulateGestureEvent(WebInputEvent::GestureFlingCancel, source_device);
  merged_event =GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureFlingCancel, merged_event.type);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(3U, GestureEventLastQueueEventSize());

  // Adding a second GFC is dropped.
  SimulateGestureEvent(WebInputEvent::GestureFlingCancel, source_device);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(3U, GestureEventLastQueueEventSize());

  // Adding another GFS will add it to the queue.
  SimulateGestureFlingStartEvent(0, -10, source_device);
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureFlingStart, merged_event.type);
  EXPECT_TRUE(FlingInProgress());
  EXPECT_EQ(4U, GestureEventLastQueueEventSize());

  // GFS in queue means that a GFC is added to the queue
  SimulateGestureEvent(WebInputEvent::GestureFlingCancel, source_device);
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureFlingCancel, merged_event.type);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(5U, GestureEventLastQueueEventSize());

  // Adding another GFC with a GFC already there is dropped.
  SimulateGestureEvent(WebInputEvent::GestureFlingCancel, source_device);
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureFlingCancel, merged_event.type);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(5U, GestureEventLastQueueEventSize());
}

INSTANTIATE_TEST_CASE_P(AllSources,
                        ImmediateInputRouterWithSourceTest,
                        testing::Values(WebGestureEvent::Touchscreen,
                                        WebGestureEvent::Touchpad));
#endif  // GTEST_HAS_PARAM_TEST

// Test that GestureTapDown events are deferred.
TEST_F(ImmediateInputRouterTest, DeferredGestureTapDown) {
  // Set some sort of short deferral timeout
  set_maximum_tap_gap_time_ms(5);

  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());

  // Wait long enough for first timeout and see if it fired.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(10));
  base::MessageLoop::current()->Run();

  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(1U, GestureEventLastQueueEventSize());
  EXPECT_EQ(WebInputEvent::GestureTapDown,
            GestureEventLastQueueEvent().type);
}

// Test that GestureTapDown events are sent immediately on GestureTap.
TEST_F(ImmediateInputRouterTest, DeferredGestureTapDownSentOnTap) {
  // Set some sort of short deferral timeout
  set_maximum_tap_gap_time_ms(5);

  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());

  SimulateGestureEvent(WebInputEvent::GestureTap,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(2U, GestureEventLastQueueEventSize());
  EXPECT_EQ(WebInputEvent::GestureTap,
            GestureEventLastQueueEvent().type);

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(10));
  base::MessageLoop::current()->Run();

  EXPECT_EQ(WebInputEvent::GestureTapDown,
            client_->immediately_sent_gesture_event().event.type);

  // If the deferral timer incorrectly fired, it sent an extra message.
  EXPECT_EQ(1U, process_->sink().message_count());
}

// Test that only a single GestureTapDown event is sent when tap occurs after
// the timeout.
TEST_F(ImmediateInputRouterTest, DeferredGestureTapDownOnlyOnce) {
  // Set some sort of short deferral timeout
  set_maximum_tap_gap_time_ms(5);

  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());

  // Wait long enough for the timeout and verify it fired.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(10));
  base::MessageLoop::current()->Run();

  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(1U, GestureEventLastQueueEventSize());
  EXPECT_EQ(WebInputEvent::GestureTapDown,
            GestureEventLastQueueEvent().type);

  // Now send the tap gesture and verify we didn't get an extra TapDown.
  SimulateGestureEvent(WebInputEvent::GestureTap,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(2U, GestureEventLastQueueEventSize());
  EXPECT_EQ(WebInputEvent::GestureTap,
            GestureEventLastQueueEvent().type);
}

// Test that scroll events during the deferral interval drop the GestureTapDown.
TEST_F(ImmediateInputRouterTest, DeferredGestureTapDownAnulledOnScroll) {
  // Set some sort of short deferral timeout
  set_maximum_tap_gap_time_ms(5);

  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());

  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(1U, GestureEventLastQueueEventSize());
  EXPECT_EQ(WebInputEvent::GestureScrollBegin,
            GestureEventLastQueueEvent().type);

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(10));
  base::MessageLoop::current()->Run();

  // If the deferral timer incorrectly fired, it will send an extra message.
  EXPECT_EQ(1U, process_->sink().message_count());
}

// Test that a tap cancel event during the deferral interval drops the
// GestureTapDown.
TEST_F(ImmediateInputRouterTest, DeferredGestureTapDownAnulledOnTapCancel) {
  // Set some sort of short deferral timeout
  set_maximum_tap_gap_time_ms(5);

  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());

  SimulateGestureEvent(WebInputEvent::GestureTapCancel,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(10));
  base::MessageLoop::current()->Run();

  // If the deferral timer incorrectly fired, it will send an extra message.
  EXPECT_EQ(0U, process_->sink().message_count());
}

// Test that if a GestureTapDown gets sent, any corresponding GestureTapCancel
// is also sent.
TEST_F(ImmediateInputRouterTest, DeferredGestureTapDownTapCancel) {
  // Set some sort of short deferral timeout
  set_maximum_tap_gap_time_ms(5);

  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, GestureEventLastQueueEventSize());

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(10));
  base::MessageLoop::current()->Run();

  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(1U, GestureEventLastQueueEventSize());

  SimulateGestureEvent(WebInputEvent::GestureTapCancel,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(2U, GestureEventLastQueueEventSize());
}

// Test that a GestureScrollEnd | GestureFlingStart are deferred during the
// debounce interval, that Scrolls are not and that the deferred events are
// sent after that timer fires.
TEST_F(ImmediateInputRouterTest, DebounceDefersFollowingGestureEvents) {
  set_debounce_interval_time_ms(3);

  SimulateGestureEvent(WebInputEvent::GestureScrollUpdate,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(1U, GestureEventLastQueueEventSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::GestureScrollUpdate,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(2U, GestureEventLastQueueEventSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(2U, GestureEventLastQueueEventSize());
  EXPECT_EQ(1U, GestureEventDebouncingQueueSize());

  SimulateGestureFlingStartEvent(0, 10, WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(2U, GestureEventLastQueueEventSize());
  EXPECT_EQ(2U, GestureEventDebouncingQueueSize());

  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(2U, GestureEventLastQueueEventSize());
  EXPECT_EQ(3U, GestureEventDebouncingQueueSize());

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(5));
  base::MessageLoop::current()->Run();

  // The deferred events are correctly queued in coalescing queue.
  EXPECT_EQ(1U, process_->sink().message_count());
  if (shouldDeferTapDownEvents())
    // NOTE: The  TapDown is still deferred hence not queued.
    EXPECT_EQ(4U, GestureEventLastQueueEventSize());
  else
    EXPECT_EQ(5U, GestureEventLastQueueEventSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_FALSE(ScrollingInProgress());

  // Verify that the coalescing queue contains the correct events.
  WebInputEvent::Type expected[] = {
      WebInputEvent::GestureScrollUpdate,
      WebInputEvent::GestureScrollUpdate,
      WebInputEvent::GestureScrollEnd,
      WebInputEvent::GestureFlingStart};

  for (unsigned i = 0; i < sizeof(expected) / sizeof(WebInputEvent::Type);
      i++) {
    WebGestureEvent merged_event = GestureEventQueueEventAt(i);
    EXPECT_EQ(expected[i], merged_event.type);
  }
}

// Test that non-scroll events are deferred while scrolling during the debounce
// interval and are discarded if a GestureScrollUpdate event arrives before the
// interval end.
TEST_F(ImmediateInputRouterTest, DebounceDropsDeferredEvents) {
  set_debounce_interval_time_ms(3);
  EXPECT_FALSE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::GestureScrollUpdate,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(1U, GestureEventLastQueueEventSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  // This event should get discarded.
  SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(1U, GestureEventLastQueueEventSize());
  EXPECT_EQ(1U, GestureEventDebouncingQueueSize());

  SimulateGestureEvent(WebInputEvent::GestureScrollUpdate,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(2U, GestureEventLastQueueEventSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  // Verify that the coalescing queue contains the correct events.
  WebInputEvent::Type expected[] = {
      WebInputEvent::GestureScrollUpdate,
      WebInputEvent::GestureScrollUpdate};

  for (unsigned i = 0; i < sizeof(expected) / sizeof(WebInputEvent::Type);
      i++) {
    WebGestureEvent merged_event = GestureEventQueueEventAt(i);
    EXPECT_EQ(expected[i], merged_event.type);
  }
}

// Tests that touch-events are queued properly.
TEST_F(ImmediateInputRouterTest, TouchEventQueue) {
  PressTouchPoint(1, 1);
  SendTouchEvent();
  client_->ExpectSendImmediatelyCalled(true);
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();

  // The second touch should not be sent since one is already in queue.
  MoveTouchPoint(0, 5, 5);
  SendTouchEvent();
  client_->ExpectSendImmediatelyCalled(false);
  EXPECT_EQ(0U, process_->sink().message_count());

  EXPECT_EQ(2U, TouchEventQueueSize());

  // Receive an ACK for the first touch-event.
  SendInputEventACK(WebInputEvent::TouchStart,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, TouchEventQueueSize());
  client_->ExpectAckCalled(1);
  EXPECT_EQ(WebInputEvent::TouchStart,
            client_->acked_touch_event().event.type);
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();

  SendInputEventACK(WebInputEvent::TouchMove,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, TouchEventQueueSize());
  client_->ExpectAckCalled(1);
  EXPECT_EQ(WebInputEvent::TouchMove,
            client_->acked_touch_event().event.type);
  EXPECT_EQ(0U, process_->sink().message_count());
}

// Tests that the touch-queue is emptied if a page stops listening for touch
// events.
TEST_F(ImmediateInputRouterTest, TouchEventQueueFlush) {
  input_router_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));
  EXPECT_TRUE(client_->has_touch_handler());
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, TouchEventQueueSize());

  EXPECT_EQ(0U, TouchEventQueueSize());
  EXPECT_TRUE(input_router_->ShouldForwardTouchEvent());

  // Send a touch-press event.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();

  ReleaseTouchPoint(0);
  SendTouchEvent();

  for (int i = 5; i < 15; ++i) {
    PressTouchPoint(1, 1);
    SendTouchEvent();
    MoveTouchPoint(0, i, i);
    SendTouchEvent();
    ReleaseTouchPoint(0);
    SendTouchEvent();
  }
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(32U, TouchEventQueueSize());

  // Receive an ACK for the first touch-event. One of the queued touch-event
  // should be forwarded.
  SendInputEventACK(WebInputEvent::TouchStart,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(31U, TouchEventQueueSize());
  EXPECT_EQ(WebInputEvent::TouchStart,
            client_->acked_touch_event().event.type);
  client_->ExpectAckCalled(1);
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();

  // The page stops listening for touch-events. The touch-event queue should now
  // be emptied, but none of the queued touch-events should be sent to the
  // renderer.
  input_router_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, false));
  EXPECT_FALSE(client_->has_touch_handler());
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, TouchEventQueueSize());
  EXPECT_FALSE(input_router_->ShouldForwardTouchEvent());
}

// Tests that touch-events are coalesced properly in the queue.
TEST_F(ImmediateInputRouterTest, TouchEventQueueCoalesce) {
  input_router_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, TouchEventQueueSize());
  EXPECT_TRUE(input_router_->ShouldForwardTouchEvent());

  // Send a touch-press event.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  client_->ExpectSendImmediatelyCalled(true);
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();

  // Send a few touch-move events, followed by a touch-release event. All the
  // touch-move events should be coalesced into a single event.
  for (int i = 5; i < 15; ++i) {
    MoveTouchPoint(0, i, i);
    SendTouchEvent();
  }
  client_->ExpectSendImmediatelyCalled(false);
  ReleaseTouchPoint(0);
  SendTouchEvent();
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(3U, TouchEventQueueSize());
  client_->ExpectSendImmediatelyCalled(false);

  // ACK the press.
  SendInputEventACK(WebInputEvent::TouchStart,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(2U, TouchEventQueueSize());
  EXPECT_EQ(WebInputEvent::TouchStart,
            client_->acked_touch_event().event.type);
  client_->ExpectAckCalled(1);
  process_->sink().ClearMessages();

  // Coalesced touch-move events should be sent.
  client_->ExpectSendImmediatelyCalled(true);
  EXPECT_EQ(WebInputEvent::TouchMove,
            client_->immediately_sent_touch_event().event.type);

  // ACK the moves.
  SendInputEventACK(WebInputEvent::TouchMove,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(1U, TouchEventQueueSize());
  EXPECT_EQ(WebInputEvent::TouchMove,
            client_->acked_touch_event().event.type);
  client_->ExpectAckCalled(10);
  process_->sink().ClearMessages();

  // ACK the release.
  SendInputEventACK(WebInputEvent::TouchEnd,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, TouchEventQueueSize());
  EXPECT_EQ(WebInputEvent::TouchEnd,
            client_->acked_touch_event().event.type);
  client_->ExpectAckCalled(1);
}

// Tests that an event that has already been sent but hasn't been ack'ed yet
// doesn't get coalesced with newer events.
TEST_F(ImmediateInputRouterTest, SentTouchEventDoesNotCoalesce) {
  input_router_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, TouchEventQueueSize());
  EXPECT_TRUE(input_router_->ShouldForwardTouchEvent());

  // Send a touch-press event.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();

  // Send a few touch-move events, followed by a touch-release event. All the
  // touch-move events should be coalesced into a single event.
  for (int i = 5; i < 15; ++i) {
    MoveTouchPoint(0, i, i);
    SendTouchEvent();
  }
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(2U, TouchEventQueueSize());

  SendInputEventACK(WebInputEvent::TouchStart,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(1U, TouchEventQueueSize());
  process_->sink().ClearMessages();

  // The coalesced touch-move event has been sent to the renderer. Any new
  // touch-move event should not be coalesced with the sent event.
  MoveTouchPoint(0, 5, 5);
  SendTouchEvent();
  EXPECT_EQ(2U, TouchEventQueueSize());

  MoveTouchPoint(0, 7, 7);
  SendTouchEvent();
  EXPECT_EQ(2U, TouchEventQueueSize());
}

// Tests that coalescing works correctly for multi-touch events.
TEST_F(ImmediateInputRouterTest, TouchEventQueueMultiTouch) {
  input_router_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, TouchEventQueueSize());
  EXPECT_TRUE(input_router_->ShouldForwardTouchEvent());

  // Press the first finger.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();

  // Move the finger.
  MoveTouchPoint(0, 5, 5);
  SendTouchEvent();
  EXPECT_EQ(2U, TouchEventQueueSize());

  // Now press a second finger.
  PressTouchPoint(2, 2);
  SendTouchEvent();
  EXPECT_EQ(3U, TouchEventQueueSize());

  // Move both fingers.
  MoveTouchPoint(0, 10, 10);
  MoveTouchPoint(1, 20, 20);
  SendTouchEvent();
  EXPECT_EQ(4U, TouchEventQueueSize());

  // Move only one finger now.
  MoveTouchPoint(0, 15, 15);
  SendTouchEvent();
  EXPECT_EQ(4U, TouchEventQueueSize());

  // Move the other finger.
  MoveTouchPoint(1, 25, 25);
  SendTouchEvent();
  EXPECT_EQ(4U, TouchEventQueueSize());

  // Make sure both fingers are marked as having been moved in the coalesced
  // event.
  const WebTouchEvent& event = latest_event();
  EXPECT_EQ(WebTouchPoint::StateMoved, event.touches[0].state);
  EXPECT_EQ(WebTouchPoint::StateMoved, event.touches[1].state);
}

// Tests that if a touch-event queue is destroyed in response to a touch-event
// in the renderer, then there is no crash when the ACK for that touch-event
// comes back.
TEST_F(ImmediateInputRouterTest, TouchEventAckAfterQueueFlushed) {
  // First, install a touch-event handler and send some touch-events to the
  // renderer.
  input_router_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, TouchEventQueueSize());
  EXPECT_TRUE(input_router_->ShouldForwardTouchEvent());

  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(1U, TouchEventQueueSize());
  process_->sink().ClearMessages();

  MoveTouchPoint(0, 10, 10);
  SendTouchEvent();
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(2U, TouchEventQueueSize());

  // Receive an ACK for the press. This should cause the queued touch-move to
  // be sent to the renderer.
  SendInputEventACK(WebInputEvent::TouchStart,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(1U, TouchEventQueueSize());
  process_->sink().ClearMessages();

  // Uninstall the touch-event handler. This will cause the queue to be flushed.
  input_router_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, false));
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, TouchEventQueueSize());

  // Now receive an ACK for the move.
  SendInputEventACK(WebInputEvent::TouchMove,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, TouchEventQueueSize());
}

// Tests that touch-move events are not sent to the renderer if the preceding
// touch-press event did not have a consumer (and consequently, did not hit the
// main thread in the renderer). Also tests that all queued/coalesced touch
// events are flushed immediately when the ACK for the touch-press comes back
// with NO_CONSUMER status.
TEST_F(ImmediateInputRouterTest, TouchEventQueueNoConsumer) {
  // The first touch-press should reach the renderer.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();

  // The second touch should not be sent since one is already in queue.
  MoveTouchPoint(0, 5, 5);
  SendTouchEvent();
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(2U, TouchEventQueueSize());

  // Receive an ACK for the first touch-event. This should release the queued
  // touch-event, but it should not be sent to the renderer.
  SendInputEventACK(WebInputEvent::TouchStart,
                    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(0U, TouchEventQueueSize());
  EXPECT_EQ(WebInputEvent::TouchMove,
            client_->acked_touch_event().event.type);
  client_->ExpectAckCalled(2);
  EXPECT_EQ(0U, process_->sink().message_count());
  process_->sink().ClearMessages();

  // Send a release event. This should not reach the renderer.
  ReleaseTouchPoint(0);
  SendTouchEvent();
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(WebInputEvent::TouchEnd,
            client_->acked_touch_event().event.type);
  client_->ExpectAckCalled(1);

  // Send a press-event, followed by move and release events, and another press
  // event, before the ACK for the first press event comes back. All of the
  // events should be queued first. After the NO_CONSUMER ack for the first
  // touch-press, all events upto the second touch-press should be flushed.
  PressTouchPoint(10, 10);
  SendTouchEvent();
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();

  MoveTouchPoint(0, 5, 5);
  SendTouchEvent();
  MoveTouchPoint(0, 6, 5);
  SendTouchEvent();
  ReleaseTouchPoint(0);
  SendTouchEvent();

  PressTouchPoint(6, 5);
  SendTouchEvent();
  EXPECT_EQ(0U, process_->sink().message_count());
  // The queue should hold the first sent touch-press event, the coalesced
  // touch-move event, the touch-end event and the second touch-press event.
  EXPECT_EQ(4U, TouchEventQueueSize());

  SendInputEventACK(WebInputEvent::TouchStart,
                    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(WebInputEvent::TouchEnd, client_->acked_touch_event().event.type);
  client_->ExpectAckCalled(4);
  EXPECT_EQ(1U, TouchEventQueueSize());
  process_->sink().ClearMessages();

  // ACK the second press event as NO_CONSUMER too.
  SendInputEventACK(WebInputEvent::TouchStart,
                    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(WebInputEvent::TouchStart, client_->acked_touch_event().event.type);
  client_->ExpectAckCalled(1);
  EXPECT_EQ(0U, TouchEventQueueSize());

  // Send a second press event. Even though the first touch had NO_CONSUMER,
  // this press event should reach the renderer.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(1U, TouchEventQueueSize());
}

TEST_F(ImmediateInputRouterTest, TouchEventQueueConsumerIgnoreMultiFinger) {
  // Press two touch points and move them around a bit. The renderer consumes
  // the events for the first touch point, but returns NO_CONSUMER_EXISTS for
  // the second touch point.

  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();

  MoveTouchPoint(0, 5, 5);
  SendTouchEvent();

  PressTouchPoint(10, 10);
  SendTouchEvent();

  MoveTouchPoint(0, 2, 2);
  SendTouchEvent();

  MoveTouchPoint(1, 4, 10);
  SendTouchEvent();

  MoveTouchPoint(0, 10, 10);
  MoveTouchPoint(1, 20, 20);
  SendTouchEvent();

  // Since the first touch-press is still pending ACK, no other event should
  // have been sent to the renderer.
  EXPECT_EQ(0U, process_->sink().message_count());
  // The queue includes the two presses, the first touch-move of the first
  // point, and a coalesced touch-move of both points.
  EXPECT_EQ(4U, TouchEventQueueSize());

  // ACK the first press as CONSUMED. This should cause the first touch-move of
  // the first touch-point to be dispatched.
  SendInputEventACK(WebInputEvent::TouchStart,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();
  EXPECT_EQ(3U, TouchEventQueueSize());

  // ACK the first move as CONSUMED.
  SendInputEventACK(WebInputEvent::TouchMove,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();
  EXPECT_EQ(2U, TouchEventQueueSize());

  // ACK the second press as NO_CONSUMER_EXISTS. This will dequeue the coalesced
  // touch-move event (which contains both touch points). Although the second
  // touch-point does not need to be sent to the renderer, the first touch-point
  // did move, and so the coalesced touch-event will be sent to the renderer.
  SendInputEventACK(WebInputEvent::TouchStart,
                    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();
  EXPECT_EQ(1U, TouchEventQueueSize());

  // ACK the coalesced move as NOT_CONSUMED.
  SendInputEventACK(WebInputEvent::TouchMove,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, TouchEventQueueSize());

  // Move just the second touch point. Because the first touch point did not
  // move, this event should not reach the renderer.
  MoveTouchPoint(1, 30, 30);
  SendTouchEvent();
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, TouchEventQueueSize());

  // Move just the first touch point. This should reach the renderer.
  MoveTouchPoint(0, 10, 10);
  SendTouchEvent();
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(1U, TouchEventQueueSize());
  process_->sink().ClearMessages();

  // Move both fingers. This event should reach the renderer (after the ACK of
  // the previous move event is received), because the first touch point did
  // move.
  MoveTouchPoint(0, 15, 15);
  MoveTouchPoint(1, 25, 25);
  SendTouchEvent();
  EXPECT_EQ(0U, process_->sink().message_count());

  SendInputEventACK(WebInputEvent::TouchMove,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(1U, TouchEventQueueSize());
  process_->sink().ClearMessages();

  SendInputEventACK(WebInputEvent::TouchMove,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, TouchEventQueueSize());

  // Release the first finger. Then move the second finger around some, then
  // press another finger. Once the release event is ACKed, the move events of
  // the second finger should be immediately released to the view, and the
  // touch-press event should be dispatched to the renderer.
  ReleaseTouchPoint(0);
  SendTouchEvent();
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(1U, TouchEventQueueSize());
  process_->sink().ClearMessages();

  MoveTouchPoint(1, 40, 40);
  SendTouchEvent();

  MoveTouchPoint(1, 50, 50);
  SendTouchEvent();

  PressTouchPoint(1, 1);
  SendTouchEvent();

  MoveTouchPoint(1, 30, 30);
  SendTouchEvent();
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(4U, TouchEventQueueSize());

  SendInputEventACK(WebInputEvent::TouchEnd,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(2U, TouchEventQueueSize());
  EXPECT_EQ(WebInputEvent::TouchMove,
            client_->acked_touch_event().event.type);
  process_->sink().ClearMessages();

  // ACK the press with NO_CONSUMED_EXISTS. This should release the queued
  // touch-move events to the view.
  SendInputEventACK(WebInputEvent::TouchStart,
                    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, TouchEventQueueSize());
  EXPECT_EQ(WebInputEvent::TouchMove,
            client_->acked_touch_event().event.type);

  ReleaseTouchPoint(2);
  ReleaseTouchPoint(1);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, TouchEventQueueSize());
}

#if defined(OS_WIN) || defined(USE_AURA)
// Tests that the acked events have correct state. (ui::Events are used only on
// windows and aura)
TEST_F(ImmediateInputRouterTest, AckedTouchEventState) {
  input_router_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, TouchEventQueueSize());
  EXPECT_TRUE(input_router_->ShouldForwardTouchEvent());

  // Send a bunch of events, and make sure the ACKed events are correct.
  ScopedVector<ui::TouchEvent> expected_events;

  // Use a custom timestamp for all the events to test that the acked events
  // have the same timestamp;
  base::TimeDelta timestamp = base::Time::NowFromSystemTime() - base::Time();
  timestamp -= base::TimeDelta::FromSeconds(600);

  // Press the first finger.
  PressTouchPoint(1, 1);
  SetTouchTimestamp(timestamp);
  SendTouchEvent();
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();
  expected_events.push_back(new ui::TouchEvent(ui::ET_TOUCH_PRESSED,
      gfx::Point(1, 1), 0, timestamp));

  // Move the finger.
  timestamp += base::TimeDelta::FromSeconds(10);
  MoveTouchPoint(0, 5, 5);
  SetTouchTimestamp(timestamp);
  SendTouchEvent();
  EXPECT_EQ(2U, TouchEventQueueSize());
  expected_events.push_back(new ui::TouchEvent(ui::ET_TOUCH_MOVED,
      gfx::Point(5, 5), 0, timestamp));

  // Now press a second finger.
  timestamp += base::TimeDelta::FromSeconds(10);
  PressTouchPoint(2, 2);
  SetTouchTimestamp(timestamp);
  SendTouchEvent();
  EXPECT_EQ(3U, TouchEventQueueSize());
  expected_events.push_back(new ui::TouchEvent(ui::ET_TOUCH_PRESSED,
      gfx::Point(2, 2), 1, timestamp));

  // Move both fingers.
  timestamp += base::TimeDelta::FromSeconds(10);
  MoveTouchPoint(0, 10, 10);
  MoveTouchPoint(1, 20, 20);
  SetTouchTimestamp(timestamp);
  SendTouchEvent();
  EXPECT_EQ(4U, TouchEventQueueSize());
  expected_events.push_back(new ui::TouchEvent(ui::ET_TOUCH_MOVED,
      gfx::Point(10, 10), 0, timestamp));
  expected_events.push_back(new ui::TouchEvent(ui::ET_TOUCH_MOVED,
      gfx::Point(20, 20), 1, timestamp));

  // Receive the ACKs and make sure the generated events from the acked events
  // are correct.
  WebInputEvent::Type acks[] = { WebInputEvent::TouchStart,
                                 WebInputEvent::TouchMove,
                                 WebInputEvent::TouchStart,
                                 WebInputEvent::TouchMove };

  TouchEventCoordinateSystem coordinate_system = LOCAL_COORDINATES;
#if !defined(OS_WIN)
  coordinate_system = SCREEN_COORDINATES;
#endif
  for (size_t i = 0; i < arraysize(acks); ++i) {
    SendInputEventACK(acks[i],
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_EQ(acks[i], client_->acked_touch_event().event.type);
    ScopedVector<ui::TouchEvent> acked;

    MakeUITouchEventsFromWebTouchEvents(
        client_->acked_touch_event(), &acked, coordinate_system);
    bool success = EventListIsSubset(acked, expected_events);
    EXPECT_TRUE(success) << "Failed on step: " << i;
    if (!success)
      break;
    expected_events.erase(expected_events.begin(),
                          expected_events.begin() + acked.size());
  }

  EXPECT_EQ(0U, expected_events.size());
}
#endif  // defined(OS_WIN) || defined(USE_AURA)

TEST_F(ImmediateInputRouterTest, UnhandledWheelEvent) {
  // Simulate wheel events.
  SimulateWheelEvent(0, -5, 0, false);  // sent directly
  SimulateWheelEvent(0, -10, 0, false);  // enqueued

  // Check that only the first event was sent.
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Indicate that the wheel event was unhandled.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // Check that the correct unhandled wheel event was received.
  client_->ExpectAckCalled(1);
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, client_->ack_state());
  EXPECT_EQ(client_->acked_wheel_event().deltaY, -5);

  // Check that the second event was sent.
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Check that the correct unhandled wheel event was received.
  EXPECT_EQ(client_->acked_wheel_event().deltaY, -5);
}

}  // namespace content
