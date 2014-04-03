// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/timer/timer.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/renderer_host/backing_store.h"
#include "content/browser/renderer_host/input/gesture_event_queue.h"
#include "content/browser/renderer_host/input/input_router_impl.h"
#include "content/browser/renderer_host/input/tap_suppression_controller.h"
#include "content/browser/renderer_host/input/tap_suppression_controller_client.h"
#include "content/browser/renderer_host/input/touch_event_queue.h"
#include "content/browser/renderer_host/overscroll_controller.h"
#include "content/browser/renderer_host/overscroll_controller_delegate.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/port/browser/render_widget_host_view_port.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/screen.h"

#if defined(USE_AURA)
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_screen.h"
#include "ui/compositor/test/in_process_context_factory.h"
#endif

#if defined(OS_WIN) || defined(USE_AURA)
#include "content/browser/renderer_host/ui_events_helper.h"
#include "ui/events/event.h"
#endif

using base::TimeDelta;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {

// TestOverscrollDelegate ------------------------------------------------------

class TestOverscrollDelegate : public OverscrollControllerDelegate {
 public:
  explicit TestOverscrollDelegate(RenderWidgetHostView* view)
      : view_(view),
        current_mode_(OVERSCROLL_NONE),
        completed_mode_(OVERSCROLL_NONE),
        delta_x_(0.f),
        delta_y_(0.f) {
  }

  virtual ~TestOverscrollDelegate() {}

  OverscrollMode current_mode() const { return current_mode_; }
  OverscrollMode completed_mode() const { return completed_mode_; }
  float delta_x() const { return delta_x_; }
  float delta_y() const { return delta_y_; }

  void Reset() {
    current_mode_ = OVERSCROLL_NONE;
    completed_mode_ = OVERSCROLL_NONE;
    delta_x_ = delta_y_ = 0.f;
  }

 private:
  // Overridden from OverscrollControllerDelegate:
  virtual gfx::Rect GetVisibleBounds() const OVERRIDE {
    return view_->IsShowing() ? view_->GetViewBounds() : gfx::Rect();
  }

  virtual void OnOverscrollUpdate(float delta_x, float delta_y) OVERRIDE {
    delta_x_ = delta_x;
    delta_y_ = delta_y;
  }

  virtual void OnOverscrollComplete(OverscrollMode overscroll_mode) OVERRIDE {
    EXPECT_EQ(current_mode_, overscroll_mode);
    completed_mode_ = overscroll_mode;
    current_mode_ = OVERSCROLL_NONE;
  }

  virtual void OnOverscrollModeChange(OverscrollMode old_mode,
                                      OverscrollMode new_mode) OVERRIDE {
    EXPECT_EQ(current_mode_, old_mode);
    current_mode_ = new_mode;
    delta_x_ = delta_y_ = 0.f;
  }

  RenderWidgetHostView* view_;
  OverscrollMode current_mode_;
  OverscrollMode completed_mode_;
  float delta_x_;
  float delta_y_;

  DISALLOW_COPY_AND_ASSIGN(TestOverscrollDelegate);
};

// MockInputRouter -------------------------------------------------------------

class MockInputRouter : public InputRouter {
 public:
  explicit MockInputRouter(InputRouterClient* client)
      : send_event_called_(false),
        sent_mouse_event_(false),
        sent_wheel_event_(false),
        sent_keyboard_event_(false),
        sent_gesture_event_(false),
        send_touch_event_not_cancelled_(false),
        message_received_(false),
        client_(client) {
  }
  virtual ~MockInputRouter() {}

  // InputRouter
  virtual void Flush() OVERRIDE {
    flush_called_ = true;
  }
  virtual bool SendInput(scoped_ptr<IPC::Message> message) OVERRIDE {
    send_event_called_ = true;
    return true;
  }
  virtual void SendMouseEvent(
      const MouseEventWithLatencyInfo& mouse_event) OVERRIDE {
    sent_mouse_event_ = true;
  }
  virtual void SendWheelEvent(
      const MouseWheelEventWithLatencyInfo& wheel_event) OVERRIDE {
    sent_wheel_event_ = true;
  }
  virtual void SendKeyboardEvent(
      const NativeWebKeyboardEvent& key_event,
      const ui::LatencyInfo& latency_info,
      bool is_shortcut) OVERRIDE {
    sent_keyboard_event_ = true;
  }
  virtual void SendGestureEvent(
      const GestureEventWithLatencyInfo& gesture_event) OVERRIDE {
    sent_gesture_event_ = true;
  }
  virtual void SendTouchEvent(
      const TouchEventWithLatencyInfo& touch_event) OVERRIDE {
    send_touch_event_not_cancelled_ =
        client_->FilterInputEvent(touch_event.event, touch_event.latency) ==
        INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
  }
  virtual const NativeWebKeyboardEvent* GetLastKeyboardEvent() const OVERRIDE {
    NOTREACHED();
    return NULL;
  }
  virtual bool ShouldForwardTouchEvent() const OVERRIDE { return true; }
  virtual void OnViewUpdated(int view_flags) OVERRIDE {}

  // IPC::Listener
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    message_received_ = true;
    return false;
  }

  bool flush_called_;
  bool send_event_called_;
  bool sent_mouse_event_;
  bool sent_wheel_event_;
  bool sent_keyboard_event_;
  bool sent_gesture_event_;
  bool send_touch_event_not_cancelled_;
  bool message_received_;

 private:
  InputRouterClient* client_;

  DISALLOW_COPY_AND_ASSIGN(MockInputRouter);
};

// MockRenderWidgetHost ----------------------------------------------------

class MockRenderWidgetHost : public RenderWidgetHostImpl {
 public:
  MockRenderWidgetHost(
      RenderWidgetHostDelegate* delegate,
      RenderProcessHost* process,
      int routing_id)
      : RenderWidgetHostImpl(delegate, process, routing_id, false),
        unresponsive_timer_fired_(false) {
    input_router_impl_ = static_cast<InputRouterImpl*>(input_router_.get());
  }

  // Allow poking at a few private members.
  using RenderWidgetHostImpl::OnUpdateRect;
  using RenderWidgetHostImpl::RendererExited;
  using RenderWidgetHostImpl::last_requested_size_;
  using RenderWidgetHostImpl::is_hidden_;
  using RenderWidgetHostImpl::resize_ack_pending_;
  using RenderWidgetHostImpl::input_router_;

  bool unresponsive_timer_fired() const {
    return unresponsive_timer_fired_;
  }

  void set_hung_renderer_delay_ms(int delay_ms) {
    hung_renderer_delay_ms_ = delay_ms;
  }

  unsigned GestureEventLastQueueEventSize() const {
    return gesture_event_queue().coalesced_gesture_events_.size();
  }

  WebGestureEvent GestureEventSecondFromLastQueueEvent() const {
    return gesture_event_queue().coalesced_gesture_events_.at(
      GestureEventLastQueueEventSize() - 2).event;
  }

  WebGestureEvent GestureEventLastQueueEvent() const {
    return gesture_event_queue().coalesced_gesture_events_.back().event;
  }

  unsigned GestureEventDebouncingQueueSize() const {
    return gesture_event_queue().debouncing_deferral_queue_.size();
  }

  WebGestureEvent GestureEventQueueEventAt(int i) const {
    return gesture_event_queue().coalesced_gesture_events_.at(i).event;
  }

  bool ScrollingInProgress() const {
    return gesture_event_queue().scrolling_in_progress_;
  }

  void SetupForOverscrollControllerTest() {
    SetOverscrollControllerEnabled(true);
    overscroll_delegate_.reset(new TestOverscrollDelegate(GetView()));
    overscroll_controller_->set_delegate(overscroll_delegate_.get());
  }

  void DisableGestureDebounce() {
    gesture_event_queue().set_debounce_enabled_for_testing(false);
  }

  void set_debounce_interval_time_ms(int delay_ms) {
    gesture_event_queue().
        set_debounce_interval_time_ms_for_testing(delay_ms);
  }

  bool TouchEventQueueEmpty() const {
    return touch_event_queue().empty();
  }

  bool ScrollStateIsContentScrolling() const {
    return scroll_state() == OverscrollController::STATE_CONTENT_SCROLLING;
  }

  bool ScrollStateIsOverscrolling() const {
    return scroll_state() == OverscrollController::STATE_OVERSCROLLING;
  }

  bool ScrollStateIsUnknown() const {
    return scroll_state() == OverscrollController::STATE_UNKNOWN;
  }

  OverscrollController::ScrollState scroll_state() const {
    return overscroll_controller_->scroll_state_;
  }

  OverscrollMode overscroll_mode() const {
    return overscroll_controller_->overscroll_mode_;
  }

  float overscroll_delta_x() const {
    return overscroll_controller_->overscroll_delta_x_;
  }

  float overscroll_delta_y() const {
    return overscroll_controller_->overscroll_delta_y_;
  }

  TestOverscrollDelegate* overscroll_delegate() {
    return overscroll_delegate_.get();
  }

  void SetupForInputRouterTest() {
    mock_input_router_ = new MockInputRouter(this);
    input_router_.reset(mock_input_router_);
  }

  MockInputRouter* mock_input_router() {
    return mock_input_router_;
  }

 protected:
  virtual void NotifyRendererUnresponsive() OVERRIDE {
    unresponsive_timer_fired_ = true;
  }

  const TouchEventQueue& touch_event_queue() const {
    return input_router_impl_->touch_event_queue_;
  }

  const GestureEventQueue& gesture_event_queue() const {
    return input_router_impl_->gesture_event_queue_;
  }

  GestureEventQueue& gesture_event_queue() {
    return input_router_impl_->gesture_event_queue_;
  }

 private:
  bool unresponsive_timer_fired_;

  // |input_router_impl_| and |mock_input_router_| are owned by
  // RenderWidgetHostImpl.  The handles below are provided for convenience so
  // that we don't have to reinterpret_cast it all the time.
  InputRouterImpl* input_router_impl_;
  MockInputRouter* mock_input_router_;

  scoped_ptr<TestOverscrollDelegate> overscroll_delegate_;

  DISALLOW_COPY_AND_ASSIGN(MockRenderWidgetHost);
};

namespace  {

// RenderWidgetHostProcess -----------------------------------------------------

class RenderWidgetHostProcess : public MockRenderProcessHost {
 public:
  explicit RenderWidgetHostProcess(BrowserContext* browser_context)
      : MockRenderProcessHost(browser_context),
        current_update_buf_(NULL),
        update_msg_should_reply_(false),
        update_msg_reply_flags_(0) {
  }
  virtual ~RenderWidgetHostProcess() {
    delete current_update_buf_;
  }

  void set_update_msg_should_reply(bool reply) {
    update_msg_should_reply_ = reply;
  }
  void set_update_msg_reply_flags(int flags) {
    update_msg_reply_flags_ = flags;
  }

  // Fills the given update parameters with resonable default values.
  void InitUpdateRectParams(ViewHostMsg_UpdateRect_Params* params);

  virtual bool HasConnection() const OVERRIDE { return true; }

 protected:
  virtual bool WaitForBackingStoreMsg(int render_widget_id,
                                      const base::TimeDelta& max_delay,
                                      IPC::Message* msg) OVERRIDE;

  TransportDIB* current_update_buf_;

  // Set to true when WaitForBackingStoreMsg should return a successful update
  // message reply. False implies timeout.
  bool update_msg_should_reply_;

  // Indicates the flags that should be sent with a repaint request. This
  // only has an effect when update_msg_should_reply_ is true.
  int update_msg_reply_flags_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostProcess);
};

void RenderWidgetHostProcess::InitUpdateRectParams(
    ViewHostMsg_UpdateRect_Params* params) {
  // Create the shared backing store.
  const int w = 100, h = 100;
  const size_t pixel_size = w * h * 4;

  if (!current_update_buf_)
    current_update_buf_ = TransportDIB::Create(pixel_size, 0);
  params->bitmap = current_update_buf_->id();
  params->bitmap_rect = gfx::Rect(0, 0, w, h);
  params->scroll_delta = gfx::Vector2d();
  params->copy_rects.push_back(params->bitmap_rect);
  params->view_size = gfx::Size(w, h);
  params->flags = update_msg_reply_flags_;
  params->needs_ack = true;
  params->scale_factor = 1;
}

bool RenderWidgetHostProcess::WaitForBackingStoreMsg(
    int render_widget_id,
    const base::TimeDelta& max_delay,
    IPC::Message* msg) {
  if (!update_msg_should_reply_)
    return false;

  // Construct a fake update reply.
  ViewHostMsg_UpdateRect_Params params;
  InitUpdateRectParams(&params);

  ViewHostMsg_UpdateRect message(render_widget_id, params);
  *msg = message;
  return true;
}

// TestView --------------------------------------------------------------------

// This test view allows us to specify the size, and keep track of acked
// touch-events.
class TestView : public TestRenderWidgetHostView {
 public:
  explicit TestView(RenderWidgetHostImpl* rwh)
      : TestRenderWidgetHostView(rwh),
        unhandled_wheel_event_count_(0),
        acked_event_count_(0),
        gesture_event_type_(-1),
        use_fake_physical_backing_size_(false),
        ack_result_(INPUT_EVENT_ACK_STATE_UNKNOWN) {
  }

  // Sets the bounds returned by GetViewBounds.
  void set_bounds(const gfx::Rect& bounds) {
    bounds_ = bounds;
  }

  const WebTouchEvent& acked_event() const { return acked_event_; }
  int acked_event_count() const { return acked_event_count_; }
  void ClearAckedEvent() {
    acked_event_.type = blink::WebInputEvent::Undefined;
    acked_event_count_ = 0;
  }

  const WebMouseWheelEvent& unhandled_wheel_event() const {
    return unhandled_wheel_event_;
  }
  int unhandled_wheel_event_count() const {
    return unhandled_wheel_event_count_;
  }
  int gesture_event_type() const { return gesture_event_type_; }
  InputEventAckState ack_result() const { return ack_result_; }

  void SetMockPhysicalBackingSize(const gfx::Size& mock_physical_backing_size) {
    use_fake_physical_backing_size_ = true;
    mock_physical_backing_size_ = mock_physical_backing_size;
  }
  void ClearMockPhysicalBackingSize() {
    use_fake_physical_backing_size_ = false;
  }

  // RenderWidgetHostView override.
  virtual gfx::Rect GetViewBounds() const OVERRIDE {
    return bounds_;
  }
  virtual void ProcessAckedTouchEvent(const TouchEventWithLatencyInfo& touch,
                                      InputEventAckState ack_result) OVERRIDE {
    acked_event_ = touch.event;
    ++acked_event_count_;
  }
  virtual void UnhandledWheelEvent(const WebMouseWheelEvent& event) OVERRIDE {
    unhandled_wheel_event_count_++;
    unhandled_wheel_event_ = event;
  }
  virtual void GestureEventAck(const WebGestureEvent& event,
                               InputEventAckState ack_result) OVERRIDE {
    gesture_event_type_ = event.type;
    ack_result_ = ack_result;
  }
  virtual gfx::Size GetPhysicalBackingSize() const OVERRIDE {
    if (use_fake_physical_backing_size_)
      return mock_physical_backing_size_;
    return TestRenderWidgetHostView::GetPhysicalBackingSize();
  }

 protected:
  WebMouseWheelEvent unhandled_wheel_event_;
  int unhandled_wheel_event_count_;
  WebTouchEvent acked_event_;
  int acked_event_count_;
  int gesture_event_type_;
  gfx::Rect bounds_;
  bool use_fake_physical_backing_size_;
  gfx::Size mock_physical_backing_size_;
  InputEventAckState ack_result_;

  DISALLOW_COPY_AND_ASSIGN(TestView);
};

// MockRenderWidgetHostDelegate --------------------------------------------

class MockRenderWidgetHostDelegate : public RenderWidgetHostDelegate {
 public:
  MockRenderWidgetHostDelegate()
      : prehandle_keyboard_event_(false),
        prehandle_keyboard_event_called_(false),
        prehandle_keyboard_event_type_(WebInputEvent::Undefined),
        unhandled_keyboard_event_called_(false),
        unhandled_keyboard_event_type_(WebInputEvent::Undefined),
        handle_wheel_event_(false),
        handle_wheel_event_called_(false) {
  }
  virtual ~MockRenderWidgetHostDelegate() {}

  // Tests that make sure we ignore keyboard event acknowledgments to events we
  // didn't send work by making sure we didn't call UnhandledKeyboardEvent().
  bool unhandled_keyboard_event_called() const {
    return unhandled_keyboard_event_called_;
  }

  WebInputEvent::Type unhandled_keyboard_event_type() const {
    return unhandled_keyboard_event_type_;
  }

  bool prehandle_keyboard_event_called() const {
    return prehandle_keyboard_event_called_;
  }

  WebInputEvent::Type prehandle_keyboard_event_type() const {
    return prehandle_keyboard_event_type_;
  }

  void set_prehandle_keyboard_event(bool handle) {
    prehandle_keyboard_event_ = handle;
  }

  void set_handle_wheel_event(bool handle) {
    handle_wheel_event_ = handle;
  }

  bool handle_wheel_event_called() {
    return handle_wheel_event_called_;
  }

 protected:
  virtual bool PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                      bool* is_keyboard_shortcut) OVERRIDE {
    prehandle_keyboard_event_type_ = event.type;
    prehandle_keyboard_event_called_ = true;
    return prehandle_keyboard_event_;
  }

  virtual void HandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) OVERRIDE {
    unhandled_keyboard_event_type_ = event.type;
    unhandled_keyboard_event_called_ = true;
  }

  virtual bool HandleWheelEvent(
      const blink::WebMouseWheelEvent& event) OVERRIDE {
    handle_wheel_event_called_ = true;
    return handle_wheel_event_;
  }

 private:
  bool prehandle_keyboard_event_;
  bool prehandle_keyboard_event_called_;
  WebInputEvent::Type prehandle_keyboard_event_type_;

  bool unhandled_keyboard_event_called_;
  WebInputEvent::Type unhandled_keyboard_event_type_;

  bool handle_wheel_event_;
  bool handle_wheel_event_called_;
};

// RenderWidgetHostTest --------------------------------------------------------

class RenderWidgetHostTest : public testing::Test {
 public:
  RenderWidgetHostTest()
      : process_(NULL),
        handle_key_press_event_(false),
        handle_mouse_event_(false) {
  }
  virtual ~RenderWidgetHostTest() {
  }

  bool KeyPressEventCallback(const NativeWebKeyboardEvent& /* event */) {
    return handle_key_press_event_;
  }
  bool MouseEventCallback(const blink::WebMouseEvent& /* event */) {
    return handle_mouse_event_;
  }

 protected:
  // testing::Test
  virtual void SetUp() {
    CommandLine* command_line = CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(switches::kValidateInputEventStream);

    browser_context_.reset(new TestBrowserContext());
    delegate_.reset(new MockRenderWidgetHostDelegate());
    process_ = new RenderWidgetHostProcess(browser_context_.get());
#if defined(USE_AURA)
    ImageTransportFactory::InitializeForUnitTests(
        scoped_ptr<ui::ContextFactory>(new ui::InProcessContextFactory));
    aura::Env::CreateInstance();
    screen_.reset(aura::TestScreen::Create());
    gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, screen_.get());
#endif
    host_.reset(
        new MockRenderWidgetHost(delegate_.get(), process_, MSG_ROUTING_NONE));
    view_.reset(new TestView(host_.get()));
    host_->SetView(view_.get());
    host_->Init();
  }
  virtual void TearDown() {
    view_.reset();
    host_.reset();
    delegate_.reset();
    process_ = NULL;
    browser_context_.reset();

#if defined(USE_AURA)
    aura::Env::DeleteInstance();
    screen_.reset();
    ImageTransportFactory::Terminate();
#endif

    // Process all pending tasks to avoid leaks.
    base::MessageLoop::current()->RunUntilIdle();
  }

  int64 GetLatencyComponentId() {
    return host_->GetLatencyComponentId();
  }

  void SendInputEventACK(WebInputEvent::Type type,
                         InputEventAckState ack_result) {
    scoped_ptr<IPC::Message> response(
        new InputHostMsg_HandleInputEvent_ACK(0, type, ack_result,
                                              ui::LatencyInfo()));
    host_->OnMessageReceived(*response);
  }

  void SimulateKeyboardEvent(WebInputEvent::Type type) {
  WebKeyboardEvent event = SyntheticWebKeyboardEventBuilder::Build(type);
  NativeWebKeyboardEvent native_event;
  memcpy(&native_event, &event, sizeof(event));
    host_->ForwardKeyboardEvent(native_event);
  }

  void SimulateMouseEvent(WebInputEvent::Type type) {
    host_->ForwardMouseEvent(SyntheticWebMouseEventBuilder::Build(type));
  }

  void SimulateMouseEventWithLatencyInfo(WebInputEvent::Type type,
                                         const ui::LatencyInfo& ui_latency) {
    host_->ForwardMouseEventWithLatencyInfo(
        SyntheticWebMouseEventBuilder::Build(type),
        ui_latency);
  }

  void SimulateWheelEvent(float dX, float dY, int modifiers, bool precise) {
    host_->ForwardWheelEvent(
        SyntheticWebMouseWheelEventBuilder::Build(dX, dY, modifiers, precise));
  }

  void SimulateWheelEventWithLatencyInfo(float dX,
                                         float dY,
                                         int modifiers,
                                         bool precise,
                                         const ui::LatencyInfo& ui_latency) {
    host_->ForwardWheelEventWithLatencyInfo(
        SyntheticWebMouseWheelEventBuilder::Build(dX, dY, modifiers, precise),
        ui_latency);
  }

  void SimulateMouseMove(int x, int y, int modifiers) {
    host_->ForwardMouseEvent(
        SyntheticWebMouseEventBuilder::Build(WebInputEvent::MouseMove,
                                             x,
                                             y,
                                             modifiers));
  }

  void SimulateWheelEventWithPhase(WebMouseWheelEvent::Phase phase) {
    host_->ForwardWheelEvent(SyntheticWebMouseWheelEventBuilder::Build(phase));
  }

  // Inject provided synthetic WebGestureEvent instance.
  void SimulateGestureEventCore(const WebGestureEvent& gesture_event) {
    host_->ForwardGestureEvent(gesture_event);
  }

  void SimulateGestureEventCoreWithLatencyInfo(
      const WebGestureEvent& gesture_event,
      const ui::LatencyInfo& ui_latency) {
    host_->ForwardGestureEventWithLatencyInfo(gesture_event, ui_latency);
  }

  // Inject simple synthetic WebGestureEvent instances.
  void SimulateGestureEvent(WebInputEvent::Type type,
                            WebGestureEvent::SourceDevice sourceDevice) {
    SimulateGestureEventCore(
        SyntheticWebGestureEventBuilder::Build(type, sourceDevice));
  }

  void SimulateGestureEventWithLatencyInfo(
      WebInputEvent::Type type,
      WebGestureEvent::SourceDevice sourceDevice,
      const ui::LatencyInfo& ui_latency) {
    SimulateGestureEventCoreWithLatencyInfo(
        SyntheticWebGestureEventBuilder::Build(type, sourceDevice),
        ui_latency);
  }

  void SimulateGestureScrollUpdateEvent(float dX, float dY, int modifiers) {
    SimulateGestureEventCore(
        SyntheticWebGestureEventBuilder::BuildScrollUpdate(dX, dY, modifiers));
  }

  void SimulateGesturePinchUpdateEvent(float scale,
                                       float anchorX,
                                       float anchorY,
                                       int modifiers) {
    SimulateGestureEventCore(
        SyntheticWebGestureEventBuilder::BuildPinchUpdate(scale,
                                                          anchorX,
                                                          anchorY,
                                                          modifiers));
  }

  // Inject synthetic GestureFlingStart events.
  void SimulateGestureFlingStartEvent(
      float velocityX,
      float velocityY,
      WebGestureEvent::SourceDevice sourceDevice) {
    SimulateGestureEventCore(
        SyntheticWebGestureEventBuilder::BuildFling(velocityX,
                                                    velocityY,
                                                    sourceDevice));
  }

  // Set the timestamp for the touch-event.
  void SetTouchTimestamp(base::TimeDelta timestamp) {
    touch_event_.SetTimestamp(timestamp);
  }

  // Sends a touch event (irrespective of whether the page has a touch-event
  // handler or not).
  void SendTouchEvent() {
    host_->ForwardTouchEventWithLatencyInfo(touch_event_, ui::LatencyInfo());

    touch_event_.ResetPoints();
  }

  int PressTouchPoint(int x, int y) {
    return touch_event_.PressPoint(x, y);
  }

  void MoveTouchPoint(int index, int x, int y) {
    touch_event_.MovePoint(index, x, y);
  }

  void ReleaseTouchPoint(int index) {
    touch_event_.ReleasePoint(index);
  }

  const WebInputEvent* GetInputEventFromMessage(const IPC::Message& message) {
    PickleIterator iter(message);
    const char* data;
    int data_length;
    if (!message.ReadData(&iter, &data, &data_length))
      return NULL;
    return reinterpret_cast<const WebInputEvent*>(data);
  }

  base::MessageLoopForUI message_loop_;

  scoped_ptr<TestBrowserContext> browser_context_;
  RenderWidgetHostProcess* process_;  // Deleted automatically by the widget.
  scoped_ptr<MockRenderWidgetHostDelegate> delegate_;
  scoped_ptr<MockRenderWidgetHost> host_;
  scoped_ptr<TestView> view_;
  scoped_ptr<gfx::Screen> screen_;
  bool handle_key_press_event_;
  bool handle_mouse_event_;

 private:
  SyntheticWebTouchEvent touch_event_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostTest);
};

#if GTEST_HAS_PARAM_TEST
// RenderWidgetHostWithSourceTest ----------------------------------------------

// This is for tests that are to be run for all source devices.
class RenderWidgetHostWithSourceTest
    : public RenderWidgetHostTest,
      public testing::WithParamInterface<WebGestureEvent::SourceDevice> {
};
#endif  // GTEST_HAS_PARAM_TEST

}  // namespace

// -----------------------------------------------------------------------------

TEST_F(RenderWidgetHostTest, Resize) {
  // The initial bounds is the empty rect, and the screen info hasn't been sent
  // yet, so setting it to the same thing shouldn't send the resize message.
  view_->set_bounds(gfx::Rect());
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Setting the bounds to a "real" rect should send out the notification.
  // but should not expect ack for empty physical backing size.
  gfx::Rect original_size(0, 0, 100, 100);
  process_->sink().ClearMessages();
  view_->set_bounds(original_size);
  view_->SetMockPhysicalBackingSize(gfx::Size());
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(original_size.size(), host_->last_requested_size_);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Setting the bounds to a "real" rect should send out the notification.
  // but should not expect ack for only physical backing size change.
  process_->sink().ClearMessages();
  view_->ClearMockPhysicalBackingSize();
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(original_size.size(), host_->last_requested_size_);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Send out a update that's not a resize ack after setting resize ack pending
  // flag. This should not clean the resize ack pending flag.
  process_->sink().ClearMessages();
  gfx::Rect second_size(0, 0, 110, 110);
  EXPECT_FALSE(host_->resize_ack_pending_);
  view_->set_bounds(second_size);
  host_->WasResized();
  EXPECT_TRUE(host_->resize_ack_pending_);
  ViewHostMsg_UpdateRect_Params params;
  process_->InitUpdateRectParams(&params);
  host_->OnUpdateRect(params);
  EXPECT_TRUE(host_->resize_ack_pending_);
  EXPECT_EQ(second_size.size(), host_->last_requested_size_);

  // Sending out a new notification should NOT send out a new IPC message since
  // a resize ACK is pending.
  gfx::Rect third_size(0, 0, 120, 120);
  process_->sink().ClearMessages();
  view_->set_bounds(third_size);
  host_->WasResized();
  EXPECT_TRUE(host_->resize_ack_pending_);
  EXPECT_EQ(second_size.size(), host_->last_requested_size_);
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Send a update that's a resize ack, but for the original_size we sent. Since
  // this isn't the second_size, the message handler should immediately send
  // a new resize message for the new size to the renderer.
  process_->sink().ClearMessages();
  params.flags = ViewHostMsg_UpdateRect_Flags::IS_RESIZE_ACK;
  params.view_size = original_size.size();
  host_->OnUpdateRect(params);
  EXPECT_TRUE(host_->resize_ack_pending_);
  EXPECT_EQ(third_size.size(), host_->last_requested_size_);
  ASSERT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Send the resize ack for the latest size.
  process_->sink().ClearMessages();
  params.view_size = third_size.size();
  host_->OnUpdateRect(params);
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(third_size.size(), host_->last_requested_size_);
  ASSERT_FALSE(process_->sink().GetFirstMessageMatching(ViewMsg_Resize::ID));

  // Now clearing the bounds should send out a notification but we shouldn't
  // expect a resize ack (since the renderer won't ack empty sizes). The message
  // should contain the new size (0x0) and not the previous one that we skipped
  process_->sink().ClearMessages();
  view_->set_bounds(gfx::Rect());
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(gfx::Size(), host_->last_requested_size_);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Send a rect that has no area but has either width or height set.
  process_->sink().ClearMessages();
  view_->set_bounds(gfx::Rect(0, 0, 0, 30));
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(gfx::Size(0, 30), host_->last_requested_size_);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Set the same size again. It should not be sent again.
  process_->sink().ClearMessages();
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(gfx::Size(0, 30), host_->last_requested_size_);
  EXPECT_FALSE(process_->sink().GetFirstMessageMatching(ViewMsg_Resize::ID));

  // A different size should be sent again, however.
  view_->set_bounds(gfx::Rect(0, 0, 0, 31));
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(gfx::Size(0, 31), host_->last_requested_size_);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));
}

// Test for crbug.com/25097.  If a renderer crashes between a resize and the
// corresponding update message, we must be sure to clear the resize ack logic.
TEST_F(RenderWidgetHostTest, ResizeThenCrash) {
  // Clear the first Resize message that carried screen info.
  process_->sink().ClearMessages();

  // Setting the bounds to a "real" rect should send out the notification.
  gfx::Rect original_size(0, 0, 100, 100);
  view_->set_bounds(original_size);
  host_->WasResized();
  EXPECT_TRUE(host_->resize_ack_pending_);
  EXPECT_EQ(original_size.size(), host_->last_requested_size_);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Simulate a renderer crash before the update message.  Ensure all the
  // resize ack logic is cleared.  Must clear the view first so it doesn't get
  // deleted.
  host_->SetView(NULL);
  host_->RendererExited(base::TERMINATION_STATUS_PROCESS_CRASHED, -1);
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(gfx::Size(), host_->last_requested_size_);

  // Reset the view so we can exit the test cleanly.
  host_->SetView(view_.get());
}

// Tests setting custom background
TEST_F(RenderWidgetHostTest, Background) {
#if !defined(OS_MACOSX)
  scoped_ptr<RenderWidgetHostView> view(
      RenderWidgetHostView::CreateViewForWidget(host_.get()));
#if defined(OS_LINUX) || defined(USE_AURA)
  // TODO(derat): Call this on all platforms: http://crbug.com/102450.
  // InitAsChild doesn't seem to work if NULL parent is passed on Windows,
  // which leads to DCHECK failure in RenderWidgetHostView::Destroy.
  // When you enable this for OS_WIN, enable |view.release()->Destroy()|
  // below.
  view->InitAsChild(NULL);
#endif
  host_->SetView(view.get());

  // Create a checkerboard background to test with.
  gfx::Canvas canvas(gfx::Size(4, 4), 1.0f, true);
  canvas.FillRect(gfx::Rect(0, 0, 2, 2), SK_ColorBLACK);
  canvas.FillRect(gfx::Rect(2, 0, 2, 2), SK_ColorWHITE);
  canvas.FillRect(gfx::Rect(0, 2, 2, 2), SK_ColorWHITE);
  canvas.FillRect(gfx::Rect(2, 2, 2, 2), SK_ColorBLACK);
  const SkBitmap& background =
      canvas.sk_canvas()->getDevice()->accessBitmap(false);

  // Set the background and make sure we get back a copy.
  view->SetBackground(background);
  EXPECT_EQ(4, view->GetBackground().width());
  EXPECT_EQ(4, view->GetBackground().height());
  EXPECT_EQ(background.getSize(), view->GetBackground().getSize());
  background.lockPixels();
  view->GetBackground().lockPixels();
  EXPECT_TRUE(0 == memcmp(background.getPixels(),
                          view->GetBackground().getPixels(),
                          background.getSize()));
  view->GetBackground().unlockPixels();
  background.unlockPixels();

  const IPC::Message* set_background =
      process_->sink().GetUniqueMessageMatching(ViewMsg_SetBackground::ID);
  ASSERT_TRUE(set_background);
  Tuple1<SkBitmap> sent_background;
  ViewMsg_SetBackground::Read(set_background, &sent_background);
  EXPECT_EQ(background.getSize(), sent_background.a.getSize());
  background.lockPixels();
  sent_background.a.lockPixels();
  EXPECT_TRUE(0 == memcmp(background.getPixels(),
                          sent_background.a.getPixels(),
                          background.getSize()));
  sent_background.a.unlockPixels();
  background.unlockPixels();

#if defined(OS_LINUX) || defined(USE_AURA)
  // See the comment above |InitAsChild(NULL)|.
  host_->SetView(NULL);
  static_cast<RenderWidgetHostViewPort*>(view.release())->Destroy();
#endif

#else
  // TODO(port): Mac does not have gfx::Canvas. Maybe we can just change this
  // test to use SkCanvas directly?
#endif

  // TODO(aa): It would be nice to factor out the painting logic so that we
  // could test that, but it appears that would mean painting everything twice
  // since windows HDC structures are opaque.
}

// Tests getting the backing store with the renderer not setting repaint ack
// flags.
TEST_F(RenderWidgetHostTest, GetBackingStore_NoRepaintAck) {
  // First set the view size to match what the renderer is rendering.
  ViewHostMsg_UpdateRect_Params params;
  process_->InitUpdateRectParams(&params);
  view_->set_bounds(gfx::Rect(params.view_size));

  // We don't currently have a backing store, and if the renderer doesn't send
  // one in time, we should get nothing.
  process_->set_update_msg_should_reply(false);
  BackingStore* backing = host_->GetBackingStore(true);
  EXPECT_FALSE(backing);
  // The widget host should have sent a request for a repaint, and there should
  // be no paint ACK.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Repaint::ID));
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(
      ViewMsg_UpdateRect_ACK::ID));

  // Allowing the renderer to reply in time should give is a backing store.
  process_->sink().ClearMessages();
  process_->set_update_msg_should_reply(true);
  process_->set_update_msg_reply_flags(0);
  backing = host_->GetBackingStore(true);
  EXPECT_TRUE(backing);
  // The widget host should NOT have sent a request for a repaint, since there
  // was an ACK already pending.
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(ViewMsg_Repaint::ID));
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
      ViewMsg_UpdateRect_ACK::ID));
}

// Tests getting the backing store with the renderer sending a repaint ack.
TEST_F(RenderWidgetHostTest, GetBackingStore_RepaintAck) {
  // First set the view size to match what the renderer is rendering.
  ViewHostMsg_UpdateRect_Params params;
  process_->InitUpdateRectParams(&params);
  view_->set_bounds(gfx::Rect(params.view_size));

  // Doing a request request with the update message allowed should work and
  // the repaint ack should work.
  process_->set_update_msg_should_reply(true);
  process_->set_update_msg_reply_flags(
      ViewHostMsg_UpdateRect_Flags::IS_REPAINT_ACK);
  BackingStore* backing = host_->GetBackingStore(true);
  EXPECT_TRUE(backing);
  // We still should not have sent out a repaint request since the last flags
  // didn't have the repaint ack set, and the pending flag will still be set.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Repaint::ID));
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
      ViewMsg_UpdateRect_ACK::ID));

  // Asking again for the backing store should just re-use the existing one
  // and not send any messagse.
  process_->sink().ClearMessages();
  backing = host_->GetBackingStore(true);
  EXPECT_TRUE(backing);
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(ViewMsg_Repaint::ID));
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(
      ViewMsg_UpdateRect_ACK::ID));
}

// Test that we don't paint when we're hidden, but we still send the ACK. Most
// of the rest of the painting is tested in the GetBackingStore* ones.
TEST_F(RenderWidgetHostTest, HiddenPaint) {
  BrowserThreadImpl ui_thread(BrowserThread::UI, base::MessageLoop::current());
  // Hide the widget, it should have sent out a message to the renderer.
  EXPECT_FALSE(host_->is_hidden_);
  host_->WasHidden();
  EXPECT_TRUE(host_->is_hidden_);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_WasHidden::ID));

  // Send it an update as from the renderer.
  process_->sink().ClearMessages();
  ViewHostMsg_UpdateRect_Params params;
  process_->InitUpdateRectParams(&params);
  host_->OnUpdateRect(params);

  // It should have sent out the ACK.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
      ViewMsg_UpdateRect_ACK::ID));

  // Now unhide.
  process_->sink().ClearMessages();
  host_->WasShown();
  EXPECT_FALSE(host_->is_hidden_);

  // It should have sent out a restored message with a request to paint.
  const IPC::Message* restored = process_->sink().GetUniqueMessageMatching(
      ViewMsg_WasShown::ID);
  ASSERT_TRUE(restored);
  Tuple1<bool> needs_repaint;
  ViewMsg_WasShown::Read(restored, &needs_repaint);
  EXPECT_TRUE(needs_repaint.a);
}

TEST_F(RenderWidgetHostTest, IgnoreKeyEventsHandledByRenderer) {
  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);

  // Make sure we sent the input event to the renderer.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::RawKeyDown,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(delegate_->unhandled_keyboard_event_called());
}

TEST_F(RenderWidgetHostTest, PreHandleRawKeyDownEvent) {
  // Simluate the situation that the browser handled the key down event during
  // pre-handle phrase.
  delegate_->set_prehandle_keyboard_event(true);
  process_->sink().ClearMessages();

  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);

  EXPECT_TRUE(delegate_->prehandle_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::RawKeyDown,
            delegate_->prehandle_keyboard_event_type());

  // Make sure the RawKeyDown event is not sent to the renderer.
  EXPECT_EQ(0U, process_->sink().message_count());

  // The browser won't pre-handle a Char event.
  delegate_->set_prehandle_keyboard_event(false);

  // Forward the Char event.
  SimulateKeyboardEvent(WebInputEvent::Char);

  // Make sure the Char event is suppressed.
  EXPECT_EQ(0U, process_->sink().message_count());

  // Forward the KeyUp event.
  SimulateKeyboardEvent(WebInputEvent::KeyUp);

  // Make sure only KeyUp was sent to the renderer.
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(InputMsg_HandleInputEvent::ID,
            process_->sink().GetMessageAt(0)->type());
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::KeyUp,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  EXPECT_TRUE(delegate_->unhandled_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::KeyUp, delegate_->unhandled_keyboard_event_type());
}

TEST_F(RenderWidgetHostTest, UnhandledWheelEvent) {
  SimulateWheelEvent(-5, 0, 0, true);

  // Make sure we sent the input event to the renderer.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(delegate_->handle_wheel_event_called());
  EXPECT_EQ(1, view_->unhandled_wheel_event_count());
  EXPECT_EQ(-5, view_->unhandled_wheel_event().deltaX);
}

TEST_F(RenderWidgetHostTest, HandleWheelEvent) {
  // Indicate that we're going to handle this wheel event
  delegate_->set_handle_wheel_event(true);

  SimulateWheelEvent(-5, 0, 0, true);

  // Make sure we sent the input event to the renderer.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // ensure the wheel event handler was invoked
  EXPECT_TRUE(delegate_->handle_wheel_event_called());

  // and that it suppressed the unhandled wheel event handler.
  EXPECT_EQ(0, view_->unhandled_wheel_event_count());
}

TEST_F(RenderWidgetHostTest, UnhandledGestureEvent) {
  SimulateGestureEvent(WebInputEvent::GestureTwoFingerTap,
                       WebGestureEvent::Touchscreen);

  // Make sure we sent the input event to the renderer.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::GestureTwoFingerTap,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(WebInputEvent::GestureTwoFingerTap, view_->gesture_event_type());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, view_->ack_result());
}

// Test that the hang monitor timer expires properly if a new timer is started
// while one is in progress (see crbug.com/11007).
TEST_F(RenderWidgetHostTest, DontPostponeHangMonitorTimeout) {
  // Start with a short timeout.
  host_->StartHangMonitorTimeout(TimeDelta::FromMilliseconds(10));

  // Immediately try to add a long 30 second timeout.
  EXPECT_FALSE(host_->unresponsive_timer_fired());
  host_->StartHangMonitorTimeout(TimeDelta::FromSeconds(30));

  // Wait long enough for first timeout and see if it fired.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(10));
  base::MessageLoop::current()->Run();
  EXPECT_TRUE(host_->unresponsive_timer_fired());
}

// Test that the hang monitor timer expires properly if it is started, stopped,
// and then started again.
TEST_F(RenderWidgetHostTest, StopAndStartHangMonitorTimeout) {
  // Start with a short timeout, then stop it.
  host_->StartHangMonitorTimeout(TimeDelta::FromMilliseconds(10));
  host_->StopHangMonitorTimeout();

  // Start it again to ensure it still works.
  EXPECT_FALSE(host_->unresponsive_timer_fired());
  host_->StartHangMonitorTimeout(TimeDelta::FromMilliseconds(10));

  // Wait long enough for first timeout and see if it fired.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(40));
  base::MessageLoop::current()->Run();
  EXPECT_TRUE(host_->unresponsive_timer_fired());
}

// Test that the hang monitor timer expires properly if it is started, then
// updated to a shorter duration.
TEST_F(RenderWidgetHostTest, ShorterDelayHangMonitorTimeout) {
  // Start with a timeout.
  host_->StartHangMonitorTimeout(TimeDelta::FromMilliseconds(100));

  // Start it again with shorter delay.
  EXPECT_FALSE(host_->unresponsive_timer_fired());
  host_->StartHangMonitorTimeout(TimeDelta::FromMilliseconds(20));

  // Wait long enough for the second timeout and see if it fired.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(25));
  base::MessageLoop::current()->Run();
  EXPECT_TRUE(host_->unresponsive_timer_fired());
}

// Test that the hang monitor catches two input events but only one ack.
// This can happen if the second input event causes the renderer to hang.
// This test will catch a regression of crbug.com/111185.
TEST_F(RenderWidgetHostTest, MultipleInputEvents) {
  // Configure the host to wait 10ms before considering
  // the renderer hung.
  host_->set_hung_renderer_delay_ms(10);

  // Send two events but only one ack.
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);
  SendInputEventACK(WebInputEvent::RawKeyDown,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  // Wait long enough for first timeout and see if it fired.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(40));
  base::MessageLoop::current()->Run();
  EXPECT_TRUE(host_->unresponsive_timer_fired());
}

// This test is not valid for Windows because getting the shared memory
// size doesn't work.
#if !defined(OS_WIN)
TEST_F(RenderWidgetHostTest, IncorrectBitmapScaleFactor) {
  ViewHostMsg_UpdateRect_Params params;
  process_->InitUpdateRectParams(&params);
  params.scale_factor = params.scale_factor * 2;

  EXPECT_EQ(0, process_->bad_msg_count());
  host_->OnUpdateRect(params);
  EXPECT_EQ(1, process_->bad_msg_count());
}
#endif

// Tests that scroll ACKs are correctly handled by the overscroll-navigation
// controller.
TEST_F(RenderWidgetHostTest, WheelScrollEventOverscrolls) {
  host_->SetupForOverscrollControllerTest();
  process_->sink().ClearMessages();

  // Simulate wheel events.
  SimulateWheelEvent(-5, 0, 0, true);  // sent directly
  SimulateWheelEvent(-1, 1, 0, true);  // enqueued
  SimulateWheelEvent(-10, -3, 0, true);  // coalesced into previous event
  SimulateWheelEvent(-15, -1, 0, true);  // coalesced into previous event
  SimulateWheelEvent(-30, -3, 0, true);  // coalesced into previous event
  SimulateWheelEvent(-20, 6, 1, true);  // enqueued, different modifiers
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();

  // Receive ACK the first wheel event as not processed.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();

  // Receive ACK for the second (coalesced) event as not processed. This will
  // start a back navigation. However, this will also cause the queued next
  // event to be sent to the renderer. But since overscroll navigation has
  // started, that event will also be included in the overscroll computation
  // instead of being sent to the renderer. So the result will be an overscroll
  // back navigation, and no event will be sent to the renderer.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_WEST, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_WEST, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(-81.f, host_->overscroll_delta_x());
  EXPECT_EQ(-31.f, host_->overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, host_->overscroll_delegate()->delta_y());
  EXPECT_EQ(0U, process_->sink().message_count());

  // Send a mouse-move event. This should cancel the overscroll navigation.
  SimulateMouseMove(5, 10, 0);
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(1U, process_->sink().message_count());
}

// Tests that if some scroll events are consumed towards the start, then
// subsequent scrolls do not horizontal overscroll.
TEST_F(RenderWidgetHostTest, WheelScrollConsumedDoNotHorizOverscroll) {
  host_->SetupForOverscrollControllerTest();
  process_->sink().ClearMessages();

  // Simulate wheel events.
  SimulateWheelEvent(-5, 0, 0, true);  // sent directly
  SimulateWheelEvent(-1, -1, 0, true);  // enqueued
  SimulateWheelEvent(-10, -3, 0, true);  // coalesced into previous event
  SimulateWheelEvent(-15, -1, 0, true);  // coalesced into previous event
  SimulateWheelEvent(-30, -3, 0, true);  // coalesced into previous event
  SimulateWheelEvent(-20, 6, 1, true);  // enqueued, different modifiers
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();

  // Receive ACK the first wheel event as processed.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();

  // Receive ACK for the second (coalesced) event as not processed. This should
  // not initiate overscroll, since the beginning of the scroll has been
  // consumed. The queued event with different modifiers should be sent to the
  // renderer.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(1U, process_->sink().message_count());

  process_->sink().ClearMessages();
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());

  // Indicate the end of the scrolling from the touchpad.
  SimulateGestureFlingStartEvent(-1200.f, 0.f, WebGestureEvent::Touchpad);
  EXPECT_EQ(1U, process_->sink().message_count());

  // Start another scroll. This time, do not consume any scroll events.
  process_->sink().ClearMessages();
  SimulateWheelEvent(0, -5, 0, true);  // sent directly
  SimulateWheelEvent(0, -1, 0, true);  // enqueued
  SimulateWheelEvent(-10, -3, 0, true);  // coalesced into previous event
  SimulateWheelEvent(-15, -1, 0, true);  // coalesced into previous event
  SimulateWheelEvent(-30, -3, 0, true);  // coalesced into previous event
  SimulateWheelEvent(-20, 6, 1, true);  // enqueued, different modifiers
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();

  // Receive ACK for the first wheel and the subsequent coalesced event as not
  // processed. This should start a back-overscroll.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_WEST, host_->overscroll_mode());
}

// Tests that wheel-scrolling correctly turns overscroll on and off.
TEST_F(RenderWidgetHostTest, WheelScrollOverscrollToggle) {
  host_->SetupForOverscrollControllerTest();
  process_->sink().ClearMessages();

  // Send a wheel event. ACK the event as not processed. This should not
  // initiate an overscroll gesture since it doesn't cross the threshold yet.
  SimulateWheelEvent(10, 0, 0, true);
  EXPECT_EQ(1U, process_->sink().message_count());
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());
  process_->sink().ClearMessages();

  // Scroll some more so as to not overscroll.
  SimulateWheelEvent(10, 0, 0, true);
  EXPECT_EQ(1U, process_->sink().message_count());
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());
  process_->sink().ClearMessages();

  // Scroll some more to initiate an overscroll.
  SimulateWheelEvent(40, 0, 0, true);
  EXPECT_EQ(1U, process_->sink().message_count());
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(60.f, host_->overscroll_delta_x());
  EXPECT_EQ(10.f, host_->overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, host_->overscroll_delegate()->delta_y());
  process_->sink().ClearMessages();

  // Scroll in the reverse direction enough to abort the overscroll.
  SimulateWheelEvent(-20, 0, 0, true);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());

  // Continue to scroll in the reverse direction.
  SimulateWheelEvent(-20, 0, 0, true);
  EXPECT_EQ(1U, process_->sink().message_count());
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());
  process_->sink().ClearMessages();

  // Continue to scroll in the reverse direction enough to initiate overscroll
  // in that direction.
  SimulateWheelEvent(-55, 0, 0, true);
  EXPECT_EQ(1U, process_->sink().message_count());
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_WEST, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_WEST, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(-75.f, host_->overscroll_delta_x());
  EXPECT_EQ(-25.f, host_->overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, host_->overscroll_delegate()->delta_y());
}

TEST_F(RenderWidgetHostTest, ScrollEventsOverscrollWithFling) {
  host_->SetupForOverscrollControllerTest();
  process_->sink().ClearMessages();

  // Send a wheel event. ACK the event as not processed. This should not
  // initiate an overscroll gesture since it doesn't cross the threshold yet.
  SimulateWheelEvent(10, 0, 0, true);
  EXPECT_EQ(1U, process_->sink().message_count());
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());
  process_->sink().ClearMessages();

  // Scroll some more so as to not overscroll.
  SimulateWheelEvent(20, 0, 0, true);
  EXPECT_EQ(1U, process_->sink().message_count());
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());
  process_->sink().ClearMessages();

  // Scroll some more to initiate an overscroll.
  SimulateWheelEvent(30, 0, 0, true);
  EXPECT_EQ(1U, process_->sink().message_count());
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(60.f, host_->overscroll_delta_x());
  EXPECT_EQ(10.f, host_->overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, host_->overscroll_delegate()->delta_y());
  process_->sink().ClearMessages();
  EXPECT_EQ(0U, host_->GestureEventLastQueueEventSize());

  // Send a fling start, but with a small velocity, so that the overscroll is
  // aborted. The fling should proceed to the renderer, through the gesture
  // event filter.
  SimulateGestureFlingStartEvent(0.f, 0.1f, WebGestureEvent::Touchpad);
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(1U, host_->GestureEventLastQueueEventSize());
  EXPECT_EQ(1U, process_->sink().message_count());
}

// Same as ScrollEventsOverscrollWithFling, but with zero velocity. Checks that
// the zero-velocity fling does not reach the renderer.
TEST_F(RenderWidgetHostTest, ScrollEventsOverscrollWithZeroFling) {
  host_->SetupForOverscrollControllerTest();
  process_->sink().ClearMessages();

  // Send a wheel event. ACK the event as not processed. This should not
  // initiate an overscroll gesture since it doesn't cross the threshold yet.
  SimulateWheelEvent(10, 0, 0, true);
  EXPECT_EQ(1U, process_->sink().message_count());
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());
  process_->sink().ClearMessages();

  // Scroll some more so as to not overscroll.
  SimulateWheelEvent(20, 0, 0, true);
  EXPECT_EQ(1U, process_->sink().message_count());
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());
  process_->sink().ClearMessages();

  // Scroll some more to initiate an overscroll.
  SimulateWheelEvent(30, 0, 0, true);
  EXPECT_EQ(1U, process_->sink().message_count());
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(60.f, host_->overscroll_delta_x());
  EXPECT_EQ(10.f, host_->overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, host_->overscroll_delegate()->delta_y());
  process_->sink().ClearMessages();
  EXPECT_EQ(0U, host_->GestureEventLastQueueEventSize());

  // Send a fling start, but with a small velocity, so that the overscroll is
  // aborted. The fling should proceed to the renderer, through the gesture
  // event filter.
  SimulateGestureFlingStartEvent(10.f, 0.f, WebGestureEvent::Touchpad);
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(1U, host_->GestureEventLastQueueEventSize());
  EXPECT_EQ(1U, process_->sink().message_count());
}

// Tests that a fling in the opposite direction of the overscroll cancels the
// overscroll nav instead of completing it.
TEST_F(RenderWidgetHostTest, ReverseFlingCancelsOverscroll) {
  host_->SetupForOverscrollControllerTest();
  host_->DisableGestureDebounce();
  process_->sink().ClearMessages();
  view_->set_bounds(gfx::Rect(0, 0, 400, 200));
  view_->Show();

  {
    // Start and end a gesture in the same direction without processing the
    // gesture events in the renderer. This should initiate and complete an
    // overscroll navigation.
    SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                         WebGestureEvent::Touchscreen);
    SimulateGestureScrollUpdateEvent(300, -5, 0);
    SendInputEventACK(WebInputEvent::GestureScrollBegin,
                      INPUT_EVENT_ACK_STATE_CONSUMED);
    SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_mode());
    EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_delegate()->current_mode());
    process_->sink().ClearMessages();

    SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                         WebGestureEvent::Touchscreen);
    EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_delegate()->completed_mode());
    EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());
    EXPECT_EQ(1U, process_->sink().message_count());
    SendInputEventACK(WebInputEvent::GestureScrollEnd,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  }

  {
    // Start over, except instead of ending the gesture with ScrollEnd, end it
    // with a FlingStart, with velocity in the reverse direction. This should
    // initiate an overscroll navigation, but it should be cancelled because of
    // the fling in the opposite direction.
    host_->overscroll_delegate()->Reset();
    SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                         WebGestureEvent::Touchscreen);
    SimulateGestureScrollUpdateEvent(-300, -5, 0);
    SendInputEventACK(WebInputEvent::GestureScrollBegin,
                      INPUT_EVENT_ACK_STATE_CONSUMED);
    SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_EQ(OVERSCROLL_WEST, host_->overscroll_mode());
    EXPECT_EQ(OVERSCROLL_WEST, host_->overscroll_delegate()->current_mode());
    process_->sink().ClearMessages();

    SimulateGestureFlingStartEvent(100, 0, WebGestureEvent::Touchscreen);
    EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->completed_mode());
    EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());
    EXPECT_EQ(1U, process_->sink().message_count());
  }
}

// Tests that touch-scroll events are handled correctly by the overscroll
// controller. This also tests that the overscroll controller and the
// gesture-event filter play nice with each other.
TEST_F(RenderWidgetHostTest, GestureScrollOverscrolls) {
  // Turn off debounce handling for test isolation.
  host_->SetupForOverscrollControllerTest();
  host_->DisableGestureDebounce();
  process_->sink().ClearMessages();

  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchscreen);
  SendInputEventACK(WebInputEvent::GestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());

  // Send another gesture event and ACK as not being processed. This should
  // initiate the navigation gesture.
  SimulateGestureScrollUpdateEvent(55, -5, 0);
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(55.f, host_->overscroll_delta_x());
  EXPECT_EQ(-5.f, host_->overscroll_delta_y());
  EXPECT_EQ(5.f, host_->overscroll_delegate()->delta_x());
  EXPECT_EQ(-5.f, host_->overscroll_delegate()->delta_y());
  EXPECT_EQ(0U, host_->GestureEventLastQueueEventSize());
  process_->sink().ClearMessages();

  // Send another gesture update event. This event should be consumed by the
  // controller, and not be forwarded to the renderer. The gesture-event filter
  // should not also receive this event.
  SimulateGestureScrollUpdateEvent(10, -5, 0);
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(65.f, host_->overscroll_delta_x());
  EXPECT_EQ(-10.f, host_->overscroll_delta_y());
  EXPECT_EQ(15.f, host_->overscroll_delegate()->delta_x());
  EXPECT_EQ(-10.f, host_->overscroll_delegate()->delta_y());
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, host_->GestureEventLastQueueEventSize());

  // Now send a scroll end. This should cancel the overscroll gesture, and send
  // the event to the renderer. The gesture-event filter should receive this
  // event.
  SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(1U, process_->sink().message_count());
  // The scroll end event will have received a synthetic ack from the input
  // router.
  EXPECT_EQ(0U, host_->GestureEventLastQueueEventSize());
}

// Tests that if the page is scrolled because of a scroll-gesture, then that
// particular scroll sequence never generates overscroll if the scroll direction
// is horizontal.
TEST_F(RenderWidgetHostTest, GestureScrollConsumedHorizontal) {
  // Turn off debounce handling for test isolation.
  host_->SetupForOverscrollControllerTest();
  host_->DisableGestureDebounce();
  process_->sink().ClearMessages();

  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchscreen);
  SimulateGestureScrollUpdateEvent(10, 0, 0);

  // Start scrolling on content. ACK both events as being processed.
  SendInputEventACK(WebInputEvent::GestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());
  process_->sink().ClearMessages();

  // Send another gesture event and ACK as not being processed. This should
  // not initiate overscroll because the beginning of the scroll event did
  // scroll some content on the page. Since there was no overscroll, the event
  // should reach the renderer.
  SimulateGestureScrollUpdateEvent(55, 0, 0);
  EXPECT_EQ(1U, process_->sink().message_count());
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
}

// Tests that the overscroll controller plays nice with touch-scrolls and the
// gesture event filter with debounce filtering turned on.
TEST_F(RenderWidgetHostTest, GestureScrollDebounceOverscrolls) {
  host_->SetupForOverscrollControllerTest();
  host_->set_debounce_interval_time_ms(100);
  process_->sink().ClearMessages();

  // Start scrolling. Receive ACK as it being processed.
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();
  SendInputEventACK(WebInputEvent::GestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  // Send update events.
  SimulateGestureScrollUpdateEvent(25, 0, 0);
  EXPECT_EQ(1U, host_->GestureEventLastQueueEventSize());
  EXPECT_EQ(0U, host_->GestureEventDebouncingQueueSize());
  EXPECT_TRUE(host_->ScrollingInProgress());
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();

  // Quickly end and restart the scroll gesture. These two events should get
  // discarded.
  SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(1U, host_->GestureEventLastQueueEventSize());
  EXPECT_EQ(1U, host_->GestureEventDebouncingQueueSize());

  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(1U, host_->GestureEventLastQueueEventSize());
  EXPECT_EQ(2U, host_->GestureEventDebouncingQueueSize());

  // Send another update event. This should get into the queue.
  SimulateGestureScrollUpdateEvent(30, 0, 0);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(2U, host_->GestureEventLastQueueEventSize());
  EXPECT_EQ(0U, host_->GestureEventDebouncingQueueSize());
  EXPECT_TRUE(host_->ScrollingInProgress());

  // Receive an ACK for the first scroll-update event as not being processed.
  // This will contribute to the overscroll gesture, but not enough for the
  // overscroll controller to start consuming gesture events. This also cause
  // the queued gesture event to be forwarded to the renderer.
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(1U, host_->GestureEventLastQueueEventSize());
  EXPECT_EQ(0U, host_->GestureEventDebouncingQueueSize());
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();

  // Send another update event. This should get into the queue.
  SimulateGestureScrollUpdateEvent(10, 0, 0);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(2U, host_->GestureEventLastQueueEventSize());
  EXPECT_EQ(0U, host_->GestureEventDebouncingQueueSize());
  EXPECT_TRUE(host_->ScrollingInProgress());

  // Receive an ACK for the second scroll-update event as not being processed.
  // This will now initiate an overscroll. This will also cause the queued
  // gesture event to be released. But instead of going to the renderer, it will
  // be consumed by the overscroll controller.
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(65.f, host_->overscroll_delta_x());
  EXPECT_EQ(15.f, host_->overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, host_->overscroll_delegate()->delta_y());
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, host_->GestureEventLastQueueEventSize());
  EXPECT_EQ(0U, host_->GestureEventDebouncingQueueSize());
}

// Tests that the gesture debounce timer plays nice with the overscroll
// controller.
TEST_F(RenderWidgetHostTest, GestureScrollDebounceTimerOverscroll) {
  host_->SetupForOverscrollControllerTest();
  host_->set_debounce_interval_time_ms(10);
  process_->sink().ClearMessages();

  // Start scrolling. Receive ACK as it being processed.
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();
  SendInputEventACK(WebInputEvent::GestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  // Send update events.
  SimulateGestureScrollUpdateEvent(55, 0, 0);
  EXPECT_EQ(1U, host_->GestureEventLastQueueEventSize());
  EXPECT_EQ(0U, host_->GestureEventDebouncingQueueSize());
  EXPECT_TRUE(host_->ScrollingInProgress());
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();

  // Send an end event. This should get in the debounce queue.
  SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(1U, host_->GestureEventLastQueueEventSize());
  EXPECT_EQ(1U, host_->GestureEventDebouncingQueueSize());

  // Receive ACK for the scroll-update event.
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(55.f, host_->overscroll_delta_x());
  EXPECT_EQ(5.f, host_->overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, host_->overscroll_delegate()->delta_y());
  EXPECT_EQ(0U, host_->GestureEventLastQueueEventSize());
  EXPECT_EQ(1U, host_->GestureEventDebouncingQueueSize());
  EXPECT_EQ(0U, process_->sink().message_count());

  // Let the timer for the debounce queue fire. That should release the queued
  // scroll-end event. Since overscroll has started, but there hasn't been
  // enough overscroll to complete the gesture, the overscroll controller
  // will reset the state. The scroll-end should therefore be dispatched to the
  // renderer, and the gesture-event-filter should await an ACK for it.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(15));
  base::MessageLoop::current()->Run();

  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());
  // The scroll end event will have received a synthetic ack from the input
  // router.
  EXPECT_EQ(0U, host_->GestureEventLastQueueEventSize());
  EXPECT_EQ(0U, host_->GestureEventDebouncingQueueSize());
  EXPECT_EQ(1U, process_->sink().message_count());
}

// Tests that when touch-events are dispatched to the renderer, the overscroll
// gesture deals with them correctly.
TEST_F(RenderWidgetHostTest, OverscrollWithTouchEvents) {
  host_->SetupForOverscrollControllerTest();
  host_->set_debounce_interval_time_ms(10);
  host_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));
  process_->sink().ClearMessages();
  view_->set_bounds(gfx::Rect(0, 0, 400, 200));
  view_->Show();

  // The test sends an intermingled sequence of touch and gesture events.

  PressTouchPoint(0, 1);
  SendTouchEvent();
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();
  SendInputEventACK(WebInputEvent::TouchStart,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  MoveTouchPoint(0, 20, 5);
  SendTouchEvent();
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();
  SendInputEventACK(WebInputEvent::TouchMove,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());

  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchscreen);
  SimulateGestureScrollUpdateEvent(20, 0, 0);
  SendInputEventACK(WebInputEvent::GestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());
  process_->sink().ClearMessages();

  // Another touch move event should reach the renderer since overscroll hasn't
  // started yet.
  MoveTouchPoint(0, 65, 10);
  SendTouchEvent();
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();

  SendInputEventACK(WebInputEvent::TouchMove,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SimulateGestureScrollUpdateEvent(45, 0, 0);
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(65.f, host_->overscroll_delta_x());
  EXPECT_EQ(15.f, host_->overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, host_->overscroll_delegate()->delta_y());
  EXPECT_TRUE(host_->TouchEventQueueEmpty());
  process_->sink().ClearMessages();

  // Send another touch event. The page should get the touch-move event, even
  // though overscroll has started.
  MoveTouchPoint(0, 55, 5);
  SendTouchEvent();
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_FALSE(host_->TouchEventQueueEmpty());
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(65.f, host_->overscroll_delta_x());
  EXPECT_EQ(15.f, host_->overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, host_->overscroll_delegate()->delta_y());

  SendInputEventACK(WebInputEvent::TouchMove,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(host_->TouchEventQueueEmpty());
  process_->sink().ClearMessages();

  SimulateGestureScrollUpdateEvent(-10, 0, 0);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_TRUE(host_->TouchEventQueueEmpty());
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(55.f, host_->overscroll_delta_x());
  EXPECT_EQ(5.f, host_->overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, host_->overscroll_delegate()->delta_y());

  MoveTouchPoint(0, 255, 5);
  SendTouchEvent();
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_FALSE(host_->TouchEventQueueEmpty());
  process_->sink().ClearMessages();
  SendInputEventACK(WebInputEvent::TouchMove,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  SimulateGestureScrollUpdateEvent(200, 0, 0);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_TRUE(host_->TouchEventQueueEmpty());
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(255.f, host_->overscroll_delta_x());
  EXPECT_EQ(205.f, host_->overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, host_->overscroll_delegate()->delta_y());

  // The touch-end/cancel event should always reach the renderer if the page has
  // touch handlers.
  ReleaseTouchPoint(0);
  SendTouchEvent();
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_FALSE(host_->TouchEventQueueEmpty());
  process_->sink().ClearMessages();

  SendInputEventACK(WebInputEvent::TouchEnd,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_TRUE(host_->TouchEventQueueEmpty());

  SimulateGestureEvent(blink::WebInputEvent::GestureScrollEnd,
                       WebGestureEvent::Touchscreen);
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(10));
  base::MessageLoop::current()->Run();
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_TRUE(host_->TouchEventQueueEmpty());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_delegate()->completed_mode());
}

// Tests that touch-gesture end is dispatched to the renderer at the end of a
// touch-gesture initiated overscroll.
TEST_F(RenderWidgetHostTest, TouchGestureEndDispatchedAfterOverscrollComplete) {
  host_->SetupForOverscrollControllerTest();
  host_->set_debounce_interval_time_ms(10);
  host_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));
  process_->sink().ClearMessages();
  view_->set_bounds(gfx::Rect(0, 0, 400, 200));
  view_->Show();

  // Start scrolling. Receive ACK as it being processed.
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, process_->sink().message_count());
  // The scroll begin event will have received a synthetic ack from the input
  // router.
  EXPECT_EQ(0U, host_->GestureEventLastQueueEventSize());
  process_->sink().ClearMessages();
  SendInputEventACK(WebInputEvent::GestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, host_->GestureEventLastQueueEventSize());
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());

  // Send update events.
  SimulateGestureScrollUpdateEvent(55, -5, 0);
  EXPECT_EQ(1U, host_->GestureEventLastQueueEventSize());
  EXPECT_EQ(0U, host_->GestureEventDebouncingQueueSize());
  EXPECT_TRUE(host_->ScrollingInProgress());
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());

  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, host_->GestureEventLastQueueEventSize());
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(55.f, host_->overscroll_delta_x());
  EXPECT_EQ(5.f, host_->overscroll_delegate()->delta_x());
  EXPECT_EQ(-5.f, host_->overscroll_delegate()->delta_y());

  // Send end event.
  SimulateGestureEvent(blink::WebInputEvent::GestureScrollEnd,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->completed_mode());
  EXPECT_EQ(0U, host_->GestureEventLastQueueEventSize());
  EXPECT_EQ(1U, host_->GestureEventDebouncingQueueSize());
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(10));
  base::MessageLoop::current()->Run();
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();
  // The scroll end event will have received a synthetic ack from the input
  // router.
  EXPECT_EQ(0U, host_->GestureEventLastQueueEventSize());
  EXPECT_EQ(0U, host_->GestureEventDebouncingQueueSize());

  SendInputEventACK(blink::WebInputEvent::GestureScrollEnd,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, host_->GestureEventLastQueueEventSize());
  EXPECT_EQ(0U, host_->GestureEventDebouncingQueueSize());

  // Start scrolling. Receive ACK as it being processed.
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, process_->sink().message_count());
  // The scroll begin event will have received a synthetic ack from the input
  // router.
  EXPECT_EQ(0U, host_->GestureEventLastQueueEventSize());
  process_->sink().ClearMessages();
  SendInputEventACK(WebInputEvent::GestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, host_->GestureEventLastQueueEventSize());
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());

  // Send update events.
  SimulateGestureScrollUpdateEvent(235, -5, 0);
  EXPECT_EQ(1U, host_->GestureEventLastQueueEventSize());
  EXPECT_EQ(0U, host_->GestureEventDebouncingQueueSize());
  EXPECT_TRUE(host_->ScrollingInProgress());
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());

  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, host_->GestureEventLastQueueEventSize());
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(235.f, host_->overscroll_delta_x());
  EXPECT_EQ(185.f, host_->overscroll_delegate()->delta_x());
  EXPECT_EQ(-5.f, host_->overscroll_delegate()->delta_y());

  // Send end event.
  SimulateGestureEvent(blink::WebInputEvent::GestureScrollEnd,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_delegate()->completed_mode());
  EXPECT_EQ(0U, host_->GestureEventLastQueueEventSize());
  EXPECT_EQ(1U, host_->GestureEventDebouncingQueueSize());

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(10));
  base::MessageLoop::current()->Run();
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();
  // The scroll end event will have received a synthetic ack from the input
  // router.
  EXPECT_EQ(0U, host_->GestureEventLastQueueEventSize());
  EXPECT_EQ(0U, host_->GestureEventDebouncingQueueSize());

  SendInputEventACK(blink::WebInputEvent::GestureScrollEnd,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(0U, host_->GestureEventLastQueueEventSize());
  EXPECT_EQ(0U, host_->GestureEventDebouncingQueueSize());
}

TEST_F(RenderWidgetHostTest, OverscrollDirectionChange) {
  host_->SetupForOverscrollControllerTest();
  host_->set_debounce_interval_time_ms(100);
  process_->sink().ClearMessages();

  // Start scrolling. Receive ACK as it being processed.
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();
  SendInputEventACK(WebInputEvent::GestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  // Send update events and receive ack as not consumed.
  SimulateGestureScrollUpdateEvent(125, -5, 0);
  EXPECT_EQ(1U, host_->GestureEventLastQueueEventSize());
  EXPECT_EQ(0U, host_->GestureEventDebouncingQueueSize());
  EXPECT_TRUE(host_->ScrollingInProgress());
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();

  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(0U, process_->sink().message_count());

  // Send another update event, but in the reverse direction. The overscroll
  // controller will consume the event, and reset the overscroll mode.
  SimulateGestureScrollUpdateEvent(-260, 0, 0);
  EXPECT_EQ(0U, process_->sink().message_count());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());

  // Since the overscroll mode has been reset, the next scroll update events
  // should reach the renderer.
  SimulateGestureScrollUpdateEvent(-20, 0, 0);
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
}

// Tests that if a mouse-move event completes the overscroll gesture, future
// move events do reach the renderer.
TEST_F(RenderWidgetHostTest, OverscrollMouseMoveCompletion) {
  host_->SetupForOverscrollControllerTest();
  host_->DisableGestureDebounce();
  process_->sink().ClearMessages();
  view_->set_bounds(gfx::Rect(0, 0, 400, 200));
  view_->Show();

  SimulateWheelEvent(5, 0, 0, true);  // sent directly
  SimulateWheelEvent(-1, 0, 0, true);  // enqueued
  SimulateWheelEvent(-10, -3, 0, true);  // coalesced into previous event
  SimulateWheelEvent(-15, -1, 0, true);  // coalesced into previous event
  SimulateWheelEvent(-30, -3, 0, true);  // coalesced into previous event
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();

  // Receive ACK the first wheel event as not processed.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();

  // Receive ACK for the second (coalesced) event as not processed. This will
  // start an overcroll gesture.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_WEST, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_WEST, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(0U, process_->sink().message_count());

  // Send a mouse-move event. This should cancel the overscroll navigation
  // (since the amount overscrolled is not above the threshold), and so the
  // mouse-move should reach the renderer.
  SimulateMouseMove(5, 10, 0);
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->completed_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();

  SendInputEventACK(WebInputEvent::MouseMove,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // Moving the mouse more should continue to send the events to the renderer.
  SimulateMouseMove(5, 10, 0);
  SendInputEventACK(WebInputEvent::MouseMove,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();

  // Now try with gestures.
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchscreen);
  SimulateGestureScrollUpdateEvent(300, -5, 0);
  SendInputEventACK(WebInputEvent::GestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_delegate()->current_mode());
  process_->sink().ClearMessages();

  // Overscroll gesture is in progress. Send a mouse-move now. This should
  // complete the gesture (because the amount overscrolled is above the
  // threshold).
  SimulateMouseMove(5, 10, 0);
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_delegate()->completed_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();
  SendInputEventACK(WebInputEvent::MouseMove,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();
  SendInputEventACK(WebInputEvent::GestureScrollEnd,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // Move mouse some more. The mouse-move events should reach the renderer.
  SimulateMouseMove(5, 10, 0);
  EXPECT_EQ(1U, process_->sink().message_count());

  SendInputEventACK(WebInputEvent::MouseMove,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  process_->sink().ClearMessages();
}

// Tests that if a page scrolled, then the overscroll controller's states are
// reset after the end of the scroll.
TEST_F(RenderWidgetHostTest, OverscrollStateResetsAfterScroll) {
  host_->SetupForOverscrollControllerTest();
  host_->DisableGestureDebounce();
  process_->sink().ClearMessages();
  view_->set_bounds(gfx::Rect(0, 0, 400, 200));
  view_->Show();

  SimulateWheelEvent(0, 5, 0, true);  // sent directly
  SimulateWheelEvent(0, 30, 0, true);  // enqueued
  SimulateWheelEvent(0, 40, 0, true);  // coalesced into previous event
  SimulateWheelEvent(0, 10, 0, true);  // coalesced into previous event
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();

  // The first wheel event is consumed. Dispatches the queued wheel event.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_TRUE(host_->ScrollStateIsContentScrolling());
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();

  // The second wheel event is consumed.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_TRUE(host_->ScrollStateIsContentScrolling());

  // Touchpad scroll can end with a zero-velocity fling. But it is not
  // dispatched, but it should still reset the overscroll controller state.
  SimulateGestureFlingStartEvent(0.f, 0.f, WebGestureEvent::Touchpad);
  EXPECT_TRUE(host_->ScrollStateIsUnknown());
  EXPECT_EQ(0U, process_->sink().message_count());

  SimulateWheelEvent(-5, 0, 0, true);  // sent directly
  SimulateWheelEvent(-60, 0, 0, true);  // enqueued
  SimulateWheelEvent(-100, 0, 0, true);  // coalesced into previous event
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_TRUE(host_->ScrollStateIsUnknown());
  process_->sink().ClearMessages();

  // The first wheel scroll did not scroll content. Overscroll should not start
  // yet, since enough hasn't been scrolled.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(host_->ScrollStateIsUnknown());
  EXPECT_EQ(1U, process_->sink().message_count());
  process_->sink().ClearMessages();

  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_WEST, host_->overscroll_mode());
  EXPECT_TRUE(host_->ScrollStateIsOverscrolling());
  EXPECT_EQ(0U, process_->sink().message_count());

  SimulateGestureFlingStartEvent(0.f, 0.f, WebGestureEvent::Touchpad);
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_WEST, host_->overscroll_delegate()->completed_mode());
  EXPECT_TRUE(host_->ScrollStateIsUnknown());
  EXPECT_EQ(0U, process_->sink().message_count());
  process_->sink().ClearMessages();
}

TEST_F(RenderWidgetHostTest, OverscrollResetsOnBlur) {
  host_->SetupForOverscrollControllerTest();
  process_->sink().ClearMessages();
  view_->set_bounds(gfx::Rect(0, 0, 400, 200));
  view_->Show();

  // Start an overscroll with gesture scroll. In the middle of the scroll, blur
  // the host.
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchscreen);
  SimulateGestureScrollUpdateEvent(300, -5, 0);
  SendInputEventACK(WebInputEvent::GestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_delegate()->current_mode());

  host_->Blur();
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->completed_mode());
  EXPECT_EQ(0.f, host_->overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, host_->overscroll_delegate()->delta_y());
  process_->sink().ClearMessages();

  SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, process_->sink().message_count());

  // Start a scroll gesture again. This should correctly start the overscroll
  // after the threshold.
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchscreen);
  SimulateGestureScrollUpdateEvent(300, -5, 0);
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_mode());
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->completed_mode());

  SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(OVERSCROLL_NONE, host_->overscroll_delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_EAST, host_->overscroll_delegate()->completed_mode());
  process_->sink().ClearMessages();
}

#define TEST_InputRouterRoutes_NOARGS(INPUTMSG) \
  TEST_F(RenderWidgetHostTest, InputRouterRoutes##INPUTMSG) { \
    host_->SetupForInputRouterTest(); \
    host_->INPUTMSG(); \
    EXPECT_TRUE(host_->mock_input_router()->send_event_called_); \
  }

TEST_InputRouterRoutes_NOARGS(Focus);
TEST_InputRouterRoutes_NOARGS(Blur);
TEST_InputRouterRoutes_NOARGS(LostCapture);

#undef TEST_InputRouterRoutes_NOARGS

#define TEST_InputRouterRoutes_NOARGS_FromRFH(INPUTMSG) \
  TEST_F(RenderWidgetHostTest, InputRouterRoutes##INPUTMSG) { \
    host_->SetupForInputRouterTest(); \
    host_->Send(new INPUTMSG(host_->GetRoutingID())); \
    EXPECT_TRUE(host_->mock_input_router()->send_event_called_); \
  }

TEST_InputRouterRoutes_NOARGS_FromRFH(InputMsg_Undo);
TEST_InputRouterRoutes_NOARGS_FromRFH(InputMsg_Redo);
TEST_InputRouterRoutes_NOARGS_FromRFH(InputMsg_Cut);
TEST_InputRouterRoutes_NOARGS_FromRFH(InputMsg_Copy);
#if defined(OS_MACOSX)
TEST_InputRouterRoutes_NOARGS_FromRFH(InputMsg_CopyToFindPboard);
#endif
TEST_InputRouterRoutes_NOARGS_FromRFH(InputMsg_Paste);
TEST_InputRouterRoutes_NOARGS_FromRFH(InputMsg_PasteAndMatchStyle);
TEST_InputRouterRoutes_NOARGS_FromRFH(InputMsg_Delete);
TEST_InputRouterRoutes_NOARGS_FromRFH(InputMsg_SelectAll);
TEST_InputRouterRoutes_NOARGS_FromRFH(InputMsg_Unselect);

#undef TEST_InputRouterRoutes_NOARGS_FromRFH

TEST_F(RenderWidgetHostTest, InputRouterRoutesReplace) {
  host_->SetupForInputRouterTest();
  host_->Send(new InputMsg_Replace(host_->GetRoutingID(), base::string16()));
  EXPECT_TRUE(host_->mock_input_router()->send_event_called_);
}

TEST_F(RenderWidgetHostTest, InputRouterRoutesReplaceMisspelling) {
  host_->SetupForInputRouterTest();
  host_->Send(new InputMsg_ReplaceMisspelling(host_->GetRoutingID(),
                                              base::string16()));
  EXPECT_TRUE(host_->mock_input_router()->send_event_called_);
}

TEST_F(RenderWidgetHostTest, IgnoreInputEvent) {
  host_->SetupForInputRouterTest();

  host_->SetIgnoreInputEvents(true);

  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);
  EXPECT_FALSE(host_->mock_input_router()->sent_keyboard_event_);

  SimulateMouseEvent(WebInputEvent::MouseMove);
  EXPECT_FALSE(host_->mock_input_router()->sent_mouse_event_);

  SimulateWheelEvent(0, 100, 0, true);
  EXPECT_FALSE(host_->mock_input_router()->sent_wheel_event_);

  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchscreen);
  EXPECT_FALSE(host_->mock_input_router()->sent_gesture_event_);

  PressTouchPoint(100, 100);
  SendTouchEvent();
  EXPECT_FALSE(host_->mock_input_router()->send_touch_event_not_cancelled_);
}

TEST_F(RenderWidgetHostTest, KeyboardListenerIgnoresEvent) {
  host_->SetupForInputRouterTest();
  host_->AddKeyPressEventCallback(
      base::Bind(&RenderWidgetHostTest::KeyPressEventCallback,
                 base::Unretained(this)));
  handle_key_press_event_ = false;
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);

  EXPECT_TRUE(host_->mock_input_router()->sent_keyboard_event_);
}

TEST_F(RenderWidgetHostTest, KeyboardListenerSuppressFollowingEvents) {
  host_->SetupForInputRouterTest();

  host_->AddKeyPressEventCallback(
      base::Bind(&RenderWidgetHostTest::KeyPressEventCallback,
                 base::Unretained(this)));

  // The callback handles the first event
  handle_key_press_event_ = true;
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);

  EXPECT_FALSE(host_->mock_input_router()->sent_keyboard_event_);

  // Following Char events should be suppressed
  handle_key_press_event_ = false;
  SimulateKeyboardEvent(WebInputEvent::Char);
  EXPECT_FALSE(host_->mock_input_router()->sent_keyboard_event_);
  SimulateKeyboardEvent(WebInputEvent::Char);
  EXPECT_FALSE(host_->mock_input_router()->sent_keyboard_event_);

  // Sending RawKeyDown event should stop suppression
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);
  EXPECT_TRUE(host_->mock_input_router()->sent_keyboard_event_);

  host_->mock_input_router()->sent_keyboard_event_ = false;
  SimulateKeyboardEvent(WebInputEvent::Char);
  EXPECT_TRUE(host_->mock_input_router()->sent_keyboard_event_);
}

TEST_F(RenderWidgetHostTest, MouseEventCallbackCanHandleEvent) {
  host_->SetupForInputRouterTest();

  host_->AddMouseEventCallback(
      base::Bind(&RenderWidgetHostTest::MouseEventCallback,
                 base::Unretained(this)));

  handle_mouse_event_ = true;
  SimulateMouseEvent(WebInputEvent::MouseDown);

  EXPECT_FALSE(host_->mock_input_router()->sent_mouse_event_);

  handle_mouse_event_ = false;
  SimulateMouseEvent(WebInputEvent::MouseDown);

  EXPECT_TRUE(host_->mock_input_router()->sent_mouse_event_);
}

TEST_F(RenderWidgetHostTest, InputRouterReceivesHandleInputEvent_ACK) {
  host_->SetupForInputRouterTest();

  SendInputEventACK(WebInputEvent::RawKeyDown,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  EXPECT_TRUE(host_->mock_input_router()->message_received_);
}

TEST_F(RenderWidgetHostTest, InputRouterReceivesMoveCaret_ACK) {
  host_->SetupForInputRouterTest();

  host_->OnMessageReceived(ViewHostMsg_MoveCaret_ACK(0));

  EXPECT_TRUE(host_->mock_input_router()->message_received_);
}

TEST_F(RenderWidgetHostTest, InputRouterReceivesSelectRange_ACK) {
  host_->SetupForInputRouterTest();

  host_->OnMessageReceived(ViewHostMsg_SelectRange_ACK(0));

  EXPECT_TRUE(host_->mock_input_router()->message_received_);
}

TEST_F(RenderWidgetHostTest, InputRouterReceivesHasTouchEventHandlers) {
  host_->SetupForInputRouterTest();

  host_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));

  EXPECT_TRUE(host_->mock_input_router()->message_received_);
}


void CheckLatencyInfoComponentInMessage(RenderWidgetHostProcess* process,
                                        int64 component_id,
                                        WebInputEvent::Type input_type) {
  const WebInputEvent* event = NULL;
  ui::LatencyInfo latency_info;
  bool is_keyboard_shortcut;
  const IPC::Message* message = process->sink().GetUniqueMessageMatching(
      InputMsg_HandleInputEvent::ID);
  ASSERT_TRUE(message);
  EXPECT_TRUE(InputMsg_HandleInputEvent::Read(
      message, &event, &latency_info, &is_keyboard_shortcut));
  EXPECT_TRUE(latency_info.FindLatency(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
      component_id,
      NULL));
  process->sink().ClearMessages();
}

// Tests that after input event passes through RWHI through ForwardXXXEvent()
// or ForwardXXXEventWithLatencyInfo(), LatencyInfo component
// ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT will always present in the
// event's LatencyInfo.
TEST_F(RenderWidgetHostTest, InputEventRWHLatencyComponent) {
  host_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));
  process_->sink().ClearMessages();

  // Tests RWHI::ForwardWheelEvent().
  SimulateWheelEvent(-5, 0, 0, true);
  CheckLatencyInfoComponentInMessage(
      process_, GetLatencyComponentId(), WebInputEvent::MouseWheel);
  SendInputEventACK(WebInputEvent::MouseWheel, INPUT_EVENT_ACK_STATE_CONSUMED);

  // Tests RWHI::ForwardWheelEventWithLatencyInfo().
  SimulateWheelEventWithLatencyInfo(-5, 0, 0, true, ui::LatencyInfo());
  CheckLatencyInfoComponentInMessage(
      process_, GetLatencyComponentId(), WebInputEvent::MouseWheel);
  SendInputEventACK(WebInputEvent::MouseWheel, INPUT_EVENT_ACK_STATE_CONSUMED);

  // Tests RWHI::ForwardMouseEvent().
  SimulateMouseEvent(WebInputEvent::MouseMove);
  CheckLatencyInfoComponentInMessage(
      process_, GetLatencyComponentId(), WebInputEvent::MouseMove);
  SendInputEventACK(WebInputEvent::MouseMove, INPUT_EVENT_ACK_STATE_CONSUMED);

  // Tests RWHI::ForwardMouseEventWithLatencyInfo().
  SimulateMouseEventWithLatencyInfo(WebInputEvent::MouseMove,
                                    ui::LatencyInfo());
  CheckLatencyInfoComponentInMessage(
      process_, GetLatencyComponentId(), WebInputEvent::MouseMove);
  SendInputEventACK(WebInputEvent::MouseMove, INPUT_EVENT_ACK_STATE_CONSUMED);

  // Tests RWHI::ForwardGestureEvent().
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchscreen);
  CheckLatencyInfoComponentInMessage(
      process_, GetLatencyComponentId(), WebInputEvent::GestureScrollBegin);
  SendInputEventACK(WebInputEvent::GestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  // Tests RWHI::ForwardGestureEventWithLatencyInfo().
  SimulateGestureEventWithLatencyInfo(WebInputEvent::GestureScrollUpdate,
                                      WebGestureEvent::Touchscreen,
                                      ui::LatencyInfo());
  CheckLatencyInfoComponentInMessage(
      process_, GetLatencyComponentId(), WebInputEvent::GestureScrollUpdate);
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  // Tests RWHI::ForwardTouchEventWithLatencyInfo().
  PressTouchPoint(0, 1);
  SendTouchEvent();
  CheckLatencyInfoComponentInMessage(
      process_, GetLatencyComponentId(), WebInputEvent::TouchStart);
  SendInputEventACK(WebInputEvent::TouchStart, INPUT_EVENT_ACK_STATE_CONSUMED);
}

}  // namespace content
