// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_mac.h"

#include <Cocoa/Cocoa.h>
#include <stddef.h>
#include <stdint.h>
#include <tuple>

#include "base/command_line.h"
#include "base/containers/queue.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/compositor/test/no_transport_image_transport_factory.h"
#include "content/browser/frame_host/render_widget_host_view_guest.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/common/input_messages.h"
#include "content/common/text_input_state.h"
#include "content/common/view_messages.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host_view_mac_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "content/test/mock_widget_impl.h"
#include "content/test/test_render_view_host.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/ocmock_extensions.h"
#import "ui/base/test/scoped_fake_nswindow_focus.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/blink_features.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/test/cocoa_test_event_utils.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"
#include "ui/latency/latency_info.h"

// Helper class with methods used to mock -[NSEvent phase], used by
// |MockScrollWheelEventWithPhase()|.
@interface MockPhaseMethods : NSObject {
}

- (NSEventPhase)phaseNone;
- (NSEventPhase)phaseBegan;
- (NSEventPhase)phaseChanged;
- (NSEventPhase)phaseEnded;
@end

@implementation MockPhaseMethods

- (NSEventPhase)phaseNone {
  return NSEventPhaseNone;
}
- (NSEventPhase)phaseBegan {
  return NSEventPhaseBegan;
}
- (NSEventPhase)phaseChanged {
  return NSEventPhaseChanged;
}
- (NSEventPhase)phaseEnded {
  return NSEventPhaseEnded;
}

@end

@interface MockRenderWidgetHostViewMacDelegate
    : NSObject<RenderWidgetHostViewMacDelegate> {
  BOOL unhandledWheelEventReceived_;
}

@property(nonatomic) BOOL unhandledWheelEventReceived;

@end

@implementation MockRenderWidgetHostViewMacDelegate

@synthesize unhandledWheelEventReceived = unhandledWheelEventReceived_;

- (void)rendererHandledWheelEvent:(const blink::WebMouseWheelEvent&)event
                         consumed:(BOOL)consumed {
  if (!consumed)
    unhandledWheelEventReceived_ = true;
}

- (void)rendererHandledGestureScrollEvent:(const blink::WebGestureEvent&)event
                                 consumed:(BOOL)consumed {
  if (!consumed &&
      event.GetType() == blink::WebInputEvent::kGestureScrollUpdate)
    unhandledWheelEventReceived_ = true;
}

- (void)touchesBeganWithEvent:(NSEvent*)event {}
- (void)touchesMovedWithEvent:(NSEvent*)event {}
- (void)touchesCancelledWithEvent:(NSEvent*)event {}
- (void)touchesEndedWithEvent:(NSEvent*)event {}
- (void)beginGestureWithEvent:(NSEvent*)event {}
- (void)endGestureWithEvent:(NSEvent*)event {}
- (void)rendererHandledOverscrollEvent:(const ui::DidOverscrollParams&)params {
}

@end

namespace content {

namespace {

std::string GetMessageNames(
    const MockWidgetInputHandler::MessageVector& events) {
  std::vector<std::string> result;
  for (auto& event : events)
    result.push_back(event->name());
  return base::JoinString(result, " ");
    }

    blink::WebPointerProperties::PointerType GetPointerType(
        const MockWidgetInputHandler::MessageVector& events) {
      EXPECT_EQ(events.size(), 1U);
      MockWidgetInputHandler::DispatchedEventMessage* event =
          events[0]->ToEvent();
      if (event && blink::WebInputEvent::IsMouseEventType(
                       event->Event()->web_event->GetType())) {
        return static_cast<const blink::WebMouseEvent*>(
                   event->Event()->web_event.get())
            ->pointer_type;
      }
      return blink::WebPointerProperties::PointerType::kUnknown;
}

NSEvent* MockTabletEventWithParams(CGEventType type,
                                   bool is_entering_proximity,
                                   NSPointingDeviceType device_type) {
  CGEventRef cg_event = CGEventCreate(NULL);
  CGEventSetType(cg_event, type);
  CGEventSetIntegerValueField(cg_event, kCGTabletProximityEventEnterProximity,
                              is_entering_proximity);
  CGEventSetIntegerValueField(cg_event, kCGTabletProximityEventPointerType,
                              device_type);
  NSEvent* event = [NSEvent eventWithCGEvent:cg_event];
  CFRelease(cg_event);
  return event;
}

NSEvent* MockMouseEventWithParams(CGEventType mouse_type,
                                  CGPoint location,
                                  CGMouseButton button,
                                  CGEventMouseSubtype subtype) {
  CGEventRef cg_event =
      CGEventCreateMouseEvent(NULL, mouse_type, location, button);
  CGEventSetIntegerValueField(cg_event, kCGMouseEventSubtype, subtype);
  NSEvent* event = [NSEvent eventWithCGEvent:cg_event];
  CFRelease(cg_event);
  return event;
}

NSEventPhase PhaseForEventType(NSEventType type) {
  if (type == NSEventTypeBeginGesture)
    return NSEventPhaseBegan;
  if (type == NSEventTypeEndGesture)
    return NSEventPhaseEnded;
  return NSEventPhaseChanged;
}

id MockGestureEvent(NSEventType type, double magnification) {
  id event = [OCMockObject mockForClass:[NSEvent class]];
  NSEventPhase phase = PhaseForEventType(type);
  NSPoint locationInWindow = NSMakePoint(0, 0);
  CGFloat deltaX = 0;
  CGFloat deltaY = 0;
  NSTimeInterval timestamp = 1;
  NSUInteger modifierFlags = 0;

  [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(type)] type];
  [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(phase)] phase];
  [(NSEvent*)[[event stub]
      andReturnValue:OCMOCK_VALUE(locationInWindow)] locationInWindow];
  [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(deltaX)] deltaX];
  [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(deltaY)] deltaY];
  [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(timestamp)] timestamp];
  [(NSEvent*)[[event stub]
      andReturnValue:OCMOCK_VALUE(modifierFlags)] modifierFlags];
  [(NSEvent*)[[event stub]
      andReturnValue:OCMOCK_VALUE(magnification)] magnification];
  return event;
}

class MockRenderWidgetHostDelegate : public RenderWidgetHostDelegate {
 public:
  MockRenderWidgetHostDelegate()
      : text_input_manager_(new TextInputManager()) {}
  ~MockRenderWidgetHostDelegate() override {}

  TextInputManager* GetTextInputManager() override {
    return text_input_manager_.get();
  }

 private:
  void ExecuteEditCommand(
      const std::string& command,
      const base::Optional<base::string16>& value) override {}
  void Cut() override {}
  void Copy() override {}
  void Paste() override {}
  void SelectAll() override {}

  std::unique_ptr<TextInputManager> text_input_manager_;

  DISALLOW_COPY_AND_ASSIGN(MockRenderWidgetHostDelegate);
};

class MockRenderWidgetHostImpl : public RenderWidgetHostImpl {
 public:
  ~MockRenderWidgetHostImpl() override {}

  // Extracts |latency_info| and stores it in |lastWheelEventLatencyInfo|.
  void ForwardWheelEventWithLatencyInfo (
        const blink::WebMouseWheelEvent& wheel_event,
        const ui::LatencyInfo& ui_latency) override {
    RenderWidgetHostImpl::ForwardWheelEventWithLatencyInfo (
        wheel_event, ui_latency);
    lastWheelEventLatencyInfo = ui::LatencyInfo(ui_latency);
  }

  MOCK_METHOD0(Focus, void());
  MOCK_METHOD0(Blur, void());

  ui::LatencyInfo lastWheelEventLatencyInfo;
  static MockRenderWidgetHostImpl* Create(RenderWidgetHostDelegate* delegate,
                                          RenderProcessHost* process,
                                          int32_t routing_id) {
    mojom::WidgetPtr widget;
    std::unique_ptr<MockWidgetImpl> widget_impl =
        std::make_unique<MockWidgetImpl>(mojo::MakeRequest(&widget));

    return new MockRenderWidgetHostImpl(delegate, process, routing_id,
                                        std::move(widget_impl),
                                        std::move(widget));
  }

  MockWidgetInputHandler* input_handler() {
    return widget_impl_->input_handler();
  }
  MockWidgetInputHandler::MessageVector GetAndResetDispatchedMessages() {
    return input_handler()->GetAndResetDispatchedMessages();
  }

 private:
  MockRenderWidgetHostImpl(RenderWidgetHostDelegate* delegate,
                           RenderProcessHost* process,
                           int32_t routing_id,
                           std::unique_ptr<MockWidgetImpl> widget_impl,
                           mojom::WidgetPtr widget)
      : RenderWidgetHostImpl(delegate,
                             process,
                             routing_id,
                             std::move(widget),
                             false),
        widget_impl_(std::move(widget_impl)) {
    set_renderer_initialized(true);
    lastWheelEventLatencyInfo = ui::LatencyInfo();

    ON_CALL(*this, Focus())
        .WillByDefault(
            testing::Invoke(this, &MockRenderWidgetHostImpl::FocusImpl));
    ON_CALL(*this, Blur())
        .WillByDefault(
            testing::Invoke(this, &MockRenderWidgetHostImpl::BlurImpl));
  }

  void FocusImpl() { RenderWidgetHostImpl::Focus(); }
  void BlurImpl() { RenderWidgetHostImpl::Blur(); }

  std::unique_ptr<MockWidgetImpl> widget_impl_;

  DISALLOW_COPY_AND_ASSIGN(MockRenderWidgetHostImpl);
};

// Generates the |length| of composition rectangle vector and save them to
// |output|. It starts from |origin| and each rectangle contains |unit_size|.
void GenerateCompositionRectArray(const gfx::Point& origin,
                                  const gfx::Size& unit_size,
                                  size_t length,
                                  const std::vector<size_t>& break_points,
                                  std::vector<gfx::Rect>* output) {
  DCHECK(output);
  output->clear();

  base::queue<int> break_point_queue;
  for (size_t i = 0; i < break_points.size(); ++i)
    break_point_queue.push(break_points[i]);
  break_point_queue.push(length);
  size_t next_break_point = break_point_queue.front();
  break_point_queue.pop();

  gfx::Rect current_rect(origin, unit_size);
  for (size_t i = 0; i < length; ++i) {
    if (i == next_break_point) {
      current_rect.set_x(origin.x());
      current_rect.set_y(current_rect.y() + current_rect.height());
      next_break_point = break_point_queue.front();
      break_point_queue.pop();
    }
    output->push_back(current_rect);
    current_rect.set_x(current_rect.right());
  }
}

gfx::Rect GetExpectedRect(const gfx::Point& origin,
                          const gfx::Size& size,
                          const gfx::Range& range,
                          int line_no) {
  return gfx::Rect(
      origin.x() + range.start() * size.width(),
      origin.y() + line_no * size.height(),
      range.length() * size.width(),
      size.height());
}

// Returns NSScrollWheel event that mocks -phase. |mockPhaseSelector| should
// correspond to a method in |MockPhaseMethods| that returns the desired phase.
NSEvent* MockScrollWheelEventWithPhase(SEL mockPhaseSelector, int32_t delta) {
  CGEventRef cg_event = CGEventCreateScrollWheelEvent(
      nullptr, kCGScrollEventUnitLine, 1, delta, 0);
  CGEventTimestamp timestamp = 0;
  CGEventSetTimestamp(cg_event, timestamp);
  NSEvent* event = [NSEvent eventWithCGEvent:cg_event];
  CFRelease(cg_event);
  method_setImplementation(
      class_getInstanceMethod([NSEvent class], @selector(phase)),
      [MockPhaseMethods instanceMethodForSelector:mockPhaseSelector]);
  return event;
}

NSEvent* MockScrollWheelEventWithoutPhase(int32_t delta) {
  return MockScrollWheelEventWithPhase(@selector(phaseNone), delta);
}

NSEvent* MockScrollWheelEventWithMomentumPhase(SEL mockPhaseSelector,
                                               int32_t delta) {
  // Create a dummy event with phaseNone. This is for resetting the phase info
  // of CGEventRef.
  MockScrollWheelEventWithPhase(@selector(phaseNone), 0);
  CGEventRef cg_event = CGEventCreateScrollWheelEvent(
      nullptr, kCGScrollEventUnitLine, 1, delta, 0);
  CGEventTimestamp timestamp = 0;
  CGEventSetTimestamp(cg_event, timestamp);
  NSEvent* event = [NSEvent eventWithCGEvent:cg_event];
  CFRelease(cg_event);
  method_setImplementation(
      class_getInstanceMethod([NSEvent class], @selector(momentumPhase)),
      [MockPhaseMethods instanceMethodForSelector:mockPhaseSelector]);
  return event;
}

}  // namespace

class RenderWidgetHostViewMacTest : public RenderViewHostImplTestHarness {
 public:
  RenderWidgetHostViewMacTest(bool scroll_latching = false)
      : rwhv_mac_(nullptr),
        scroll_latching_(scroll_latching),
        old_rwhv_(nullptr) {
    std::unique_ptr<base::SimpleTestTickClock> mock_clock(
        new base::SimpleTestTickClock());
    mock_clock->Advance(base::TimeDelta::FromMilliseconds(100));
    ui::SetEventTickClockForTesting(std::move(mock_clock));
    if (scroll_latching)
      EnableWheelScrollLatching();
    else
      DisableWheelScrollLatching();

    mojo_feature_list_.InitAndEnableFeature(features::kMojoInputMessages);
    vsync_feature_list_.InitAndEnableFeature(
        features::kVsyncAlignedInputEvents);
  }

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    gpu::ImageTransportSurface::SetAllowOSMesaForTesting(true);

    // TestRenderViewHost's destruction assumes that its view is a
    // TestRenderWidgetHostView, so store its view and reset it back to the
    // stored view in |TearDown()|.
    old_rwhv_ = rvh()->GetWidget()->GetView();

    // Owned by its |cocoa_view()|, i.e. |rwhv_cocoa_|.
    rwhv_mac_ = new RenderWidgetHostViewMac(rvh()->GetWidget(), false);
    RenderWidgetHostImpl::From(rvh()->GetWidget())->SetView(rwhv_mac_);

    rwhv_cocoa_.reset([rwhv_mac_->cocoa_view() retain]);
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    // Make sure the rwhv_mac_ is gone once the superclass's |TearDown()| runs.
    rwhv_cocoa_.reset();
    RecycleAndWait();

    // See comment in SetUp().
    test_rvh()->GetWidget()->SetView(
        static_cast<RenderWidgetHostViewBase*>(old_rwhv_));

    RenderViewHostImplTestHarness::TearDown();
  }

  void RecycleAndWait() {
    pool_.Recycle();
    base::RunLoop().RunUntilIdle();
    pool_.Recycle();
  }

  void DestroyHostViewRetainCocoaView() {
    test_rvh()->GetWidget()->SetView(nullptr);
    rwhv_mac_->Destroy();
  }

  void ActivateViewWithTextInputManager(RenderWidgetHostViewBase* view,
                                        ui::TextInputType type) {
    TextInputState state;
    state.type = type;
    view->TextInputStateChanged(state);
  }

 protected:
  std::string selected_text() const {
    return base::UTF16ToUTF8(rwhv_mac_->GetTextSelection()->selected_text());
  }

  void EnableWheelScrollLatching() {
    feature_list_.InitFromCommandLine(
        features::kTouchpadAndWheelScrollLatching.name, "");
  }

  void DisableWheelScrollLatching() {
    feature_list_.InitFromCommandLine(
        "", features::kTouchpadAndWheelScrollLatching.name);
  }

  void IgnoreEmptyUnhandledWheelEventWithWheelGestures();
  void ScrollWheelEndEventDelivery();

  RenderWidgetHostViewMac* rwhv_mac_;
  base::scoped_nsobject<RenderWidgetHostViewCocoa> rwhv_cocoa_;
  bool scroll_latching_;

 private:
  // This class isn't derived from PlatformTest.
  base::mac::ScopedNSAutoreleasePool pool_;

  RenderWidgetHostView* old_rwhv_;

  base::test::ScopedFeatureList mojo_feature_list_;
  base::test::ScopedFeatureList vsync_feature_list_;
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewMacTest);
};

TEST_F(RenderWidgetHostViewMacTest, Basic) {
}

TEST_F(RenderWidgetHostViewMacTest, AcceptsFirstResponder) {
  // The RWHVCocoa should normally accept first responder status.
  EXPECT_TRUE([rwhv_cocoa_.get() acceptsFirstResponder]);
}

// This test verifies that RenderWidgetHostViewCocoa's implementation of
// NSTextInputClientConformance conforms to requirements.
TEST_F(RenderWidgetHostViewMacTest, NSTextInputClientConformance) {
  NSRange selectedRange = [rwhv_cocoa_ selectedRange];
  EXPECT_EQ(0u, selectedRange.location);
  EXPECT_EQ(0u, selectedRange.length);

  NSRange actualRange = NSMakeRange(1u, 2u);
  NSAttributedString* actualString = [rwhv_cocoa_
      attributedSubstringForProposedRange:NSMakeRange(NSNotFound, 0u)
                              actualRange:&actualRange];
  EXPECT_EQ(nil, actualString);
  EXPECT_EQ(static_cast<NSUInteger>(NSNotFound), actualRange.location);
  EXPECT_EQ(0u, actualRange.length);

  actualString = [rwhv_cocoa_
      attributedSubstringForProposedRange:NSMakeRange(NSNotFound, 15u)
                              actualRange:&actualRange];
  EXPECT_EQ(nil, actualString);
  EXPECT_EQ(static_cast<NSUInteger>(NSNotFound), actualRange.location);
  EXPECT_EQ(0u, actualRange.length);
}

TEST_F(RenderWidgetHostViewMacTest, Fullscreen) {
  rwhv_mac_->InitAsFullscreen(nullptr);
  EXPECT_TRUE(rwhv_mac_->pepper_fullscreen_window());

  // Break the reference cycle caused by pepper_fullscreen_window() without
  // an <esc> event. See comment in
  // release_pepper_fullscreen_window_for_testing().
  rwhv_mac_->release_pepper_fullscreen_window_for_testing();
}

// Verify that escape key down in fullscreen mode suppressed the keyup event on
// the parent.
TEST_F(RenderWidgetHostViewMacTest, FullscreenCloseOnEscape) {
  // Use our own RWH since we need to destroy it.
  MockRenderWidgetHostDelegate delegate;
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  int32_t routing_id = process_host->GetNextRoutingID();
  // Owned by its |cocoa_view()|.
  MockRenderWidgetHostImpl* rwh =
      MockRenderWidgetHostImpl::Create(&delegate, process_host, routing_id);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(rwh, false);

  view->InitAsFullscreen(rwhv_mac_);

  WindowedNotificationObserver observer(
      NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,
      Source<RenderWidgetHost>(rwh));
  EXPECT_FALSE([rwhv_mac_->cocoa_view() suppressNextEscapeKeyUp]);

  // Escape key down. Should close window and set |suppressNextEscapeKeyUp| on
  // the parent.
  [view->cocoa_view() keyEvent:
      cocoa_test_event_utils::KeyEventWithKeyCode(53, 27, NSKeyDown, 0)];
  observer.Wait();
  EXPECT_TRUE([rwhv_mac_->cocoa_view() suppressNextEscapeKeyUp]);

  // Escape key up on the parent should clear |suppressNextEscapeKeyUp|.
  [rwhv_mac_->cocoa_view() keyEvent:
      cocoa_test_event_utils::KeyEventWithKeyCode(53, 27, NSKeyUp, 0)];
  EXPECT_FALSE([rwhv_mac_->cocoa_view() suppressNextEscapeKeyUp]);
}

// Test that command accelerators which destroy the fullscreen window
// don't crash when forwarded via the window's responder machinery.
TEST_F(RenderWidgetHostViewMacTest, AcceleratorDestroy) {
  // Use our own RWH since we need to destroy it.
  MockRenderWidgetHostDelegate delegate;
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  int32_t routing_id = process_host->GetNextRoutingID();
  // Owned by its |cocoa_view()|.
  MockRenderWidgetHostImpl* rwh =
      MockRenderWidgetHostImpl::Create(&delegate, process_host, routing_id);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(rwh, false);

  view->InitAsFullscreen(rwhv_mac_);

  WindowedNotificationObserver observer(
      NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,
      Source<RenderWidgetHost>(rwh));

  // Key equivalents are only sent to the renderer if the window is key.
  ui::test::ScopedFakeNSWindowFocus key_window_faker;
  [[view->cocoa_view() window] makeKeyWindow];

  // Command-ESC will destroy the view, while the window is still in
  // |-performKeyEquivalent:|.  There are other cases where this can
  // happen, Command-ESC is the easiest to trigger.
  [[view->cocoa_view() window] performKeyEquivalent:
      cocoa_test_event_utils::KeyEventWithKeyCode(
          53, 27, NSKeyDown, NSCommandKeyMask)];
  observer.Wait();
}

// Test that NSEvent of private use character won't generate keypress event
// http://crbug.com/459089
TEST_F(RenderWidgetHostViewMacTest, FilterNonPrintableCharacter) {
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  process_host->Init();
  MockRenderWidgetHostDelegate delegate;
  int32_t routing_id = process_host->GetNextRoutingID();
  MockRenderWidgetHostImpl* host =
      MockRenderWidgetHostImpl::Create(&delegate, process_host, routing_id);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(host, false);
  base::RunLoop().RunUntilIdle();

  // Simulate ctrl+F12, will produce a private use character but shouldn't
  // fire keypress event
  MockWidgetInputHandler::MessageVector events =
      host->GetAndResetDispatchedMessages();

  EXPECT_EQ(0U, events.size());
  [view->cocoa_view() keyEvent:
      cocoa_test_event_utils::KeyEventWithKeyCode(
          0x7B, 0xF70F, NSKeyDown, NSControlKeyMask)];
  base::RunLoop().RunUntilIdle();
  events = host->GetAndResetDispatchedMessages();

  EXPECT_EQ("RawKeyDown", GetMessageNames(events));

  // Simulate ctrl+delete, will produce a private use character but shouldn't
  // fire keypress event
  process_host->sink().ClearMessages();
  EXPECT_EQ(0U, process_host->sink().message_count());
  [view->cocoa_view() keyEvent:
      cocoa_test_event_utils::KeyEventWithKeyCode(
          0x2E, 0xF728, NSKeyDown, NSControlKeyMask)];
  base::RunLoop().RunUntilIdle();
  events = host->GetAndResetDispatchedMessages();
  EXPECT_EQ("RawKeyDown", GetMessageNames(events));

  // Simulate a printable char, should generate keypress event
  [view->cocoa_view() keyEvent:
      cocoa_test_event_utils::KeyEventWithKeyCode(
          0x58, 'x', NSKeyDown, NSControlKeyMask)];
  base::RunLoop().RunUntilIdle();
  events = host->GetAndResetDispatchedMessages();
  EXPECT_EQ("RawKeyDown Char", GetMessageNames(events));

  // Clean up.
  host->ShutdownAndDestroyWidget(true);
}

// Test that invalid |keyCode| shouldn't generate key events.
// https://crbug.com/601964
TEST_F(RenderWidgetHostViewMacTest, InvalidKeyCode) {
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  process_host->Init();
  MockRenderWidgetHostDelegate delegate;
  int32_t routing_id = process_host->GetNextRoutingID();
  MockRenderWidgetHostImpl* host =
      MockRenderWidgetHostImpl::Create(&delegate, process_host, routing_id);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(host, false);
  base::RunLoop().RunUntilIdle();
  // Simulate "Convert" key on JIS PC keyboard, will generate a |NSFlagsChanged|
  // NSEvent with |keyCode| == 0xFF.
  [view->cocoa_view() keyEvent:cocoa_test_event_utils::KeyEventWithKeyCode(
                                   0xFF, 0, NSFlagsChanged, 0)];
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0U, host->GetAndResetDispatchedMessages().size());

  // Clean up.
  host->ShutdownAndDestroyWidget(true);
}

TEST_F(RenderWidgetHostViewMacTest, GetFirstRectForCharacterRangeCaretCase) {
  const base::string16 kDummyString = base::UTF8ToUTF16("hogehoge");
  const size_t kDummyOffset = 0;

  gfx::Rect caret_rect(10, 11, 0, 10);
  gfx::Range caret_range(0, 0);
  ViewHostMsg_SelectionBounds_Params params;

  NSRect rect;
  NSRange actual_range;
  rwhv_mac_->SelectionChanged(kDummyString, kDummyOffset, caret_range);
  params.anchor_rect = params.focus_rect = caret_rect;
  params.anchor_dir = params.focus_dir = blink::kWebTextDirectionLeftToRight;
  rwhv_mac_->SelectionBoundsChanged(params);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        caret_range.ToNSRange(),
        &rect,
        &actual_range));
  EXPECT_EQ(caret_rect, gfx::Rect(NSRectToCGRect(rect)));
  EXPECT_EQ(caret_range, gfx::Range(actual_range));

  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        gfx::Range(0, 1).ToNSRange(),
        &rect,
        &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        gfx::Range(1, 1).ToNSRange(),
        &rect,
        &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        gfx::Range(2, 3).ToNSRange(),
        &rect,
        &actual_range));

  // Caret moved.
  caret_rect = gfx::Rect(20, 11, 0, 10);
  caret_range = gfx::Range(1, 1);
  params.anchor_rect = params.focus_rect = caret_rect;
  rwhv_mac_->SelectionChanged(kDummyString, kDummyOffset, caret_range);
  rwhv_mac_->SelectionBoundsChanged(params);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        caret_range.ToNSRange(),
        &rect,
        &actual_range));
  EXPECT_EQ(caret_rect, gfx::Rect(NSRectToCGRect(rect)));
  EXPECT_EQ(caret_range, gfx::Range(actual_range));

  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        gfx::Range(0, 0).ToNSRange(),
        &rect,
        &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        gfx::Range(1, 2).ToNSRange(),
        &rect,
        &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        gfx::Range(2, 3).ToNSRange(),
        &rect,
        &actual_range));

  // No caret.
  caret_range = gfx::Range(1, 2);
  rwhv_mac_->SelectionChanged(kDummyString, kDummyOffset, caret_range);
  params.anchor_rect = caret_rect;
  params.focus_rect = gfx::Rect(30, 11, 0, 10);
  rwhv_mac_->SelectionBoundsChanged(params);
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        gfx::Range(0, 0).ToNSRange(),
        &rect,
        &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        gfx::Range(0, 1).ToNSRange(),
        &rect,
        &actual_range));
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        gfx::Range(1, 1).ToNSRange(),
        &rect,
        &actual_range));
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        gfx::Range(1, 2).ToNSRange(),
        &rect,
        &actual_range));
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        gfx::Range(2, 2).ToNSRange(),
        &rect,
        &actual_range));
}

TEST_F(RenderWidgetHostViewMacTest, UpdateCompositionSinglelineCase) {
  ActivateViewWithTextInputManager(rwhv_mac_, ui::TEXT_INPUT_TYPE_TEXT);
  const gfx::Point kOrigin(10, 11);
  const gfx::Size kBoundsUnit(10, 20);

  NSRect rect;
  // Make sure not crashing by passing nullptr pointer instead of
  // |actual_range|.
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(0, 0).ToNSRange(), &rect, nullptr));

  // If there are no update from renderer, always returned caret position.
  NSRange actual_range;
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(0, 0).ToNSRange(),
      &rect,
      &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(0, 1).ToNSRange(),
      &rect,
      &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(1, 0).ToNSRange(),
      &rect,
      &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(1, 1).ToNSRange(),
      &rect,
      &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(1, 2).ToNSRange(),
      &rect,
      &actual_range));

  // If the firstRectForCharacterRange is failed in renderer, empty rect vector
  // is sent. Make sure this does not crash.
  rwhv_mac_->ImeCompositionRangeChanged(gfx::Range(10, 12),
                                        std::vector<gfx::Rect>());
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(10, 11).ToNSRange(), &rect, nullptr));

  const int kCompositionLength = 10;
  std::vector<gfx::Rect> composition_bounds;
  const int kCompositionStart = 3;
  const gfx::Range kCompositionRange(kCompositionStart,
                                    kCompositionStart + kCompositionLength);
  GenerateCompositionRectArray(kOrigin,
                               kBoundsUnit,
                               kCompositionLength,
                               std::vector<size_t>(),
                               &composition_bounds);
  rwhv_mac_->ImeCompositionRangeChanged(kCompositionRange, composition_bounds);

  // Out of range requests will return caret position.
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(0, 0).ToNSRange(),
      &rect,
      &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(1, 1).ToNSRange(),
      &rect,
      &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(1, 2).ToNSRange(),
      &rect,
      &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(2, 2).ToNSRange(),
      &rect,
      &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(13, 14).ToNSRange(),
      &rect,
      &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(14, 15).ToNSRange(),
      &rect,
      &actual_range));

  for (int i = 0; i <= kCompositionLength; ++i) {
    for (int j = 0; j <= kCompositionLength - i; ++j) {
      const gfx::Range range(i, i + j);
      const gfx::Rect expected_rect = GetExpectedRect(kOrigin,
                                                      kBoundsUnit,
                                                      range,
                                                      0);
      const NSRange request_range = gfx::Range(
          kCompositionStart + range.start(),
          kCompositionStart + range.end()).ToNSRange();
      EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
            request_range,
            &rect,
            &actual_range));
      EXPECT_EQ(gfx::Range(request_range), gfx::Range(actual_range));
      EXPECT_EQ(expected_rect, gfx::Rect(NSRectToCGRect(rect)));

      // Make sure not crashing by passing nullptr pointer instead of
      // |actual_range|.
      EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
          request_range, &rect, nullptr));
    }
  }
}

TEST_F(RenderWidgetHostViewMacTest, UpdateCompositionMultilineCase) {
  ActivateViewWithTextInputManager(rwhv_mac_, ui::TEXT_INPUT_TYPE_TEXT);
  const gfx::Point kOrigin(10, 11);
  const gfx::Size kBoundsUnit(10, 20);
  NSRect rect;

  const int kCompositionLength = 30;
  std::vector<gfx::Rect> composition_bounds;
  const gfx::Range kCompositionRange(0, kCompositionLength);
  // Set breaking point at 10 and 20.
  std::vector<size_t> break_points;
  break_points.push_back(10);
  break_points.push_back(20);
  GenerateCompositionRectArray(kOrigin,
                               kBoundsUnit,
                               kCompositionLength,
                               break_points,
                               &composition_bounds);
  rwhv_mac_->ImeCompositionRangeChanged(kCompositionRange, composition_bounds);

  // Range doesn't contain line breaking point.
  gfx::Range range;
  range = gfx::Range(5, 8);
  NSRange actual_range;
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(range, gfx::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, range, 0),
      gfx::Rect(NSRectToCGRect(rect)));
  range = gfx::Range(15, 18);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(range, gfx::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(5, 8), 1),
      gfx::Rect(NSRectToCGRect(rect)));
  range = gfx::Range(25, 28);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(range, gfx::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(5, 8), 2),
      gfx::Rect(NSRectToCGRect(rect)));

  // Range contains line breaking point.
  range = gfx::Range(8, 12);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(gfx::Range(8, 10), gfx::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(8, 10), 0),
      gfx::Rect(NSRectToCGRect(rect)));
  range = gfx::Range(18, 22);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(gfx::Range(18, 20), gfx::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(8, 10), 1),
      gfx::Rect(NSRectToCGRect(rect)));

  // Start point is line breaking point.
  range = gfx::Range(10, 12);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(gfx::Range(10, 12), gfx::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(0, 2), 1),
      gfx::Rect(NSRectToCGRect(rect)));
  range = gfx::Range(20, 22);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(gfx::Range(20, 22), gfx::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(0, 2), 2),
      gfx::Rect(NSRectToCGRect(rect)));

  // End point is line breaking point.
  range = gfx::Range(5, 10);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(gfx::Range(5, 10), gfx::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(5, 10), 0),
      gfx::Rect(NSRectToCGRect(rect)));
  range = gfx::Range(15, 20);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(gfx::Range(15, 20), gfx::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(5, 10), 1),
      gfx::Rect(NSRectToCGRect(rect)));

  // Start and end point are same line breaking point.
  range = gfx::Range(10, 10);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(gfx::Range(10, 10), gfx::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(0, 0), 1),
      gfx::Rect(NSRectToCGRect(rect)));
  range = gfx::Range(20, 20);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(gfx::Range(20, 20), gfx::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(0, 0), 2),
      gfx::Rect(NSRectToCGRect(rect)));

  // Start and end point are different line breaking point.
  range = gfx::Range(10, 20);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(gfx::Range(10, 20), gfx::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(0, 10), 1),
      gfx::Rect(NSRectToCGRect(rect)));
}

// Check that events coming from AppKit via -[NSTextInputClient
// firstRectForCharacterRange:actualRange] are handled in a sane manner if they
// arrive after the C++ RenderWidgetHostView is destroyed.
TEST_F(RenderWidgetHostViewMacTest, CompositionEventAfterDestroy) {
  ActivateViewWithTextInputManager(rwhv_mac_, ui::TEXT_INPUT_TYPE_TEXT);
  const gfx::Rect composition_bounds(0, 0, 30, 40);
  const gfx::Range range(0, 1);
  rwhv_mac_->ImeCompositionRangeChanged(
      range, std::vector<gfx::Rect>(1, composition_bounds));

  NSRange actual_range = NSMakeRange(0, 0);

  base::scoped_nsobject<CocoaTestHelperWindow> window(
      [[CocoaTestHelperWindow alloc] init]);
  [[window contentView] addSubview:rwhv_cocoa_];
  [rwhv_cocoa_ setFrame:NSMakeRect(0, 0, 400, 400)];

  NSRect rect = [rwhv_cocoa_ firstRectForCharacterRange:range.ToNSRange()
                                            actualRange:&actual_range];
  EXPECT_EQ(30, rect.size.width);
  EXPECT_EQ(40, rect.size.height);
  EXPECT_EQ(range, gfx::Range(actual_range));

  DestroyHostViewRetainCocoaView();
  actual_range = NSMakeRange(0, 0);
  rect = [rwhv_cocoa_ firstRectForCharacterRange:range.ToNSRange()
                                     actualRange:&actual_range];
  EXPECT_NSEQ(NSZeroRect, rect);
  EXPECT_EQ(gfx::Range(), gfx::Range(actual_range));
}

// Verify that |SetActive()| calls |RenderWidgetHostImpl::Blur()| and
// |RenderWidgetHostImp::Focus()|.
TEST_F(RenderWidgetHostViewMacTest, BlurAndFocusOnSetActive) {
  MockRenderWidgetHostDelegate delegate;
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  process_host->Init();

  // Owned by its |cocoa_view()|.
  int32_t routing_id = process_host->GetNextRoutingID();
  MockRenderWidgetHostImpl* rwh =
      MockRenderWidgetHostImpl::Create(&delegate, process_host, routing_id);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(rwh, false);

  base::scoped_nsobject<CocoaTestHelperWindow> window(
      [[CocoaTestHelperWindow alloc] init]);
  [[window contentView] addSubview:view->cocoa_view()];

  EXPECT_CALL(*rwh, Focus());
  [window makeFirstResponder:view->cocoa_view()];
  testing::Mock::VerifyAndClearExpectations(rwh);

  EXPECT_CALL(*rwh, Blur());
  view->SetActive(false);
  testing::Mock::VerifyAndClearExpectations(rwh);

  EXPECT_CALL(*rwh, Focus());
  view->SetActive(true);
  testing::Mock::VerifyAndClearExpectations(rwh);

  // Unsetting first responder should blur.
  EXPECT_CALL(*rwh, Blur());
  [window makeFirstResponder:nil];
  testing::Mock::VerifyAndClearExpectations(rwh);

  // |SetActive()| shoud not focus if view is not first responder.
  EXPECT_CALL(*rwh, Focus()).Times(0);
  view->SetActive(true);
  testing::Mock::VerifyAndClearExpectations(rwh);

  // Clean up.
  rwh->ShutdownAndDestroyWidget(true);
}

TEST_F(RenderWidgetHostViewMacTest, LastWheelEventLatencyInfoExists) {
  // Initialize the view associated with a MockRenderWidgetHostImpl, rather than
  // the MockRenderProcessHost that is set up by the test harness which mocks
  // out |OnMessageReceived()|.
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  process_host->Init();
  MockRenderWidgetHostDelegate delegate;
  int32_t routing_id = process_host->GetNextRoutingID();
  MockRenderWidgetHostImpl* host =
      MockRenderWidgetHostImpl::Create(&delegate, process_host, routing_id);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(host, false);
  process_host->sink().ClearMessages();

  // Send an initial wheel event for scrolling by 3 lines.
  // Verifies that ui::INPUT_EVENT_LATENCY_UI_COMPONENT is added
  // properly in scrollWheel function.
  NSEvent* wheelEvent1 = MockScrollWheelEventWithPhase(@selector(phaseBegan),3);
  [view->cocoa_view() scrollWheel:wheelEvent1];
  ui::LatencyInfo::LatencyComponent ui_component1;
  ASSERT_TRUE(host->lastWheelEventLatencyInfo.FindLatency(
      ui::INPUT_EVENT_LATENCY_UI_COMPONENT, 0, &ui_component1) );

  // Send a wheel event with phaseEnded.
  // Verifies that ui::INPUT_EVENT_LATENCY_UI_COMPONENT is added
  // properly in shortCircuitScrollWheelEvent function which is called
  // in scrollWheel.
  NSEvent* wheelEvent2 = MockScrollWheelEventWithPhase(@selector(phaseEnded),0);
  [view->cocoa_view() scrollWheel:wheelEvent2];
  ui::LatencyInfo::LatencyComponent ui_component2;
  ASSERT_TRUE(host->lastWheelEventLatencyInfo.FindLatency(
      ui::INPUT_EVENT_LATENCY_UI_COMPONENT, 0, &ui_component2) );

  // Clean up.
  host->ShutdownAndDestroyWidget(true);
}

TEST_F(RenderWidgetHostViewMacTest, SourceEventTypeExistsInLatencyInfo) {
  // Initialize the view associated with a MockRenderWidgetHostImpl, rather than
  // the MockRenderProcessHost that is set up by the test harness which mocks
  // out |OnMessageReceived()|.
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  process_host->Init();
  MockRenderWidgetHostDelegate delegate;
  int32_t routing_id = process_host->GetNextRoutingID();
  MockRenderWidgetHostImpl* host =
      MockRenderWidgetHostImpl::Create(&delegate, process_host, routing_id);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(host, false);
  process_host->sink().ClearMessages();

  // Send a wheel event for scrolling by 3 lines.
  // Verifies that SourceEventType exists in forwarded LatencyInfo object.
  NSEvent* wheelEvent = MockScrollWheelEventWithPhase(@selector(phaseBegan), 3);
  [view->cocoa_view() scrollWheel:wheelEvent];
  ASSERT_TRUE(host->lastWheelEventLatencyInfo.source_event_type() ==
              ui::SourceEventType::WHEEL);

  // Clean up.
  host->ShutdownAndDestroyWidget(true);
}

void RenderWidgetHostViewMacTest::ScrollWheelEndEventDelivery() {
  // Initialize the view associated with a MockRenderWidgetHostImpl, rather than
  // the MockRenderProcessHost that is set up by the test harness which mocks
  // out |OnMessageReceived()|.
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  process_host->Init();
  MockRenderWidgetHostDelegate delegate;
  int32_t routing_id = process_host->GetNextRoutingID();
  MockRenderWidgetHostImpl* host =
      MockRenderWidgetHostImpl::Create(&delegate, process_host, routing_id);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(host, false);

  // Send an initial wheel event with NSEventPhaseBegan to the view.
  NSEvent* event1 = MockScrollWheelEventWithPhase(@selector(phaseBegan), 0);
  [view->cocoa_view() scrollWheel:event1];

  // Flush and clear other messages (e.g. begin frames) the RWHVMac also sends.
  base::RunLoop().RunUntilIdle();

  MockWidgetInputHandler::MessageVector events =
      host->GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel", GetMessageNames(events));
  // Send an ACK for the first wheel event, so that the queue will be flushed.
  events[0]->ToEvent()->CallCallback(INPUT_EVENT_ACK_STATE_CONSUMED);

  // Post the NSEventPhaseEnded wheel event to NSApp and check whether the
  // render view receives it.
  NSEvent* event2 = MockScrollWheelEventWithPhase(@selector(phaseEnded), 0);
  [NSApp postEvent:event2 atStart:NO];
  base::RunLoop().RunUntilIdle();
  events = host->GetAndResetDispatchedMessages();
  if (scroll_latching_) {
    // The wheel event with phaseEnded won't be sent to the render view
    // immediately, instead the mouse_wheel_phase_handler will wait for 100ms
    // to see if a wheel event with momentumPhase began arrives or not.
    ASSERT_EQ(0U, events.size());
  } else {
    ASSERT_EQ(1U, events.size());
  }

  // Clean up.
  host->ShutdownAndDestroyWidget(true);
}
TEST_F(RenderWidgetHostViewMacTest, ScrollWheelEndEventDelivery) {
  ScrollWheelEndEventDelivery();
}

TEST_F(RenderWidgetHostViewMacTest, PointerEventWithEraserType) {
  // Initialize the view associated with a MockRenderWidgetHostImpl, rather than
  // the MockRenderProcessHost that is set up by the test harness which mocks
  // out |OnMessageReceived()|.
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  process_host->Init();
  MockRenderWidgetHostDelegate delegate;
  int32_t routing_id = process_host->GetNextRoutingID();
  MockRenderWidgetHostImpl* host =
      MockRenderWidgetHostImpl::Create(&delegate, process_host, routing_id);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(host, false);
  base::RunLoop().RunUntilIdle();

  // Send a NSEvent of NSTabletProximity type which has a device type of eraser.
  NSEvent* event = MockTabletEventWithParams(kCGEventTabletProximity, true,
                                             NSEraserPointingDevice);
  [view->cocoa_view() tabletEvent:event];
  // Flush and clear other messages (e.g. begin frames) the RWHVMac also sends.
  base::RunLoop().RunUntilIdle();

  event =
      MockMouseEventWithParams(kCGEventMouseMoved, {6, 9}, kCGMouseButtonLeft,
                               kCGEventMouseSubtypeTabletPoint);
  [view->cocoa_view() mouseEvent:event];
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      host->GetAndResetDispatchedMessages();
  ASSERT_EQ("MouseLeave", GetMessageNames(events));
  EXPECT_EQ(blink::WebPointerProperties::PointerType::kEraser,
            GetPointerType(events));

  // Clean up.
  host->ShutdownAndDestroyWidget(true);
}

TEST_F(RenderWidgetHostViewMacTest, PointerEventWithPenType) {
  // Initialize the view associated with a MockRenderWidgetHostImpl, rather than
  // the MockRenderProcessHost that is set up by the test harness which mocks
  // out |OnMessageReceived()|.
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  process_host->Init();
  MockRenderWidgetHostDelegate delegate;
  int32_t routing_id = process_host->GetNextRoutingID();
  MockRenderWidgetHostImpl* host =
      MockRenderWidgetHostImpl::Create(&delegate, process_host, routing_id);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(host, false);
  process_host->sink().ClearMessages();

  // Send a NSEvent of NSTabletProximity type which has a device type of pen.
  NSEvent* event = MockTabletEventWithParams(kCGEventTabletProximity, true,
                                             NSPenPointingDevice);
  [view->cocoa_view() tabletEvent:event];
  // Flush and clear other messages (e.g. begin frames) the RWHVMac also sends.
  base::RunLoop().RunUntilIdle();

  event =
      MockMouseEventWithParams(kCGEventMouseMoved, {6, 9}, kCGMouseButtonLeft,
                               kCGEventMouseSubtypeTabletPoint);
  [view->cocoa_view() mouseEvent:event];
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      host->GetAndResetDispatchedMessages();
  ASSERT_EQ("MouseLeave", GetMessageNames(events));
  EXPECT_EQ(blink::WebPointerProperties::PointerType::kPen,
            GetPointerType(events));

  // Clean up.
  host->ShutdownAndDestroyWidget(true);
}

TEST_F(RenderWidgetHostViewMacTest, PointerEventWithMouseType) {
  // Initialize the view associated with a MockRenderWidgetHostImpl, rather than
  // the MockRenderProcessHost that is set up by the test harness which mocks
  // out |OnMessageReceived()|.
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  process_host->Init();
  MockRenderWidgetHostDelegate delegate;
  int32_t routing_id = process_host->GetNextRoutingID();
  MockRenderWidgetHostImpl* host =
      MockRenderWidgetHostImpl::Create(&delegate, process_host, routing_id);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(host, false);
  process_host->sink().ClearMessages();

  // Send a NSEvent of a mouse type.
  NSEvent* event =
      MockMouseEventWithParams(kCGEventMouseMoved, {6, 9}, kCGMouseButtonLeft,
                               kCGEventMouseSubtypeDefault);
  [view->cocoa_view() mouseEvent:event];
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      host->GetAndResetDispatchedMessages();
  ASSERT_EQ("MouseLeave", GetMessageNames(events));
  EXPECT_EQ(blink::WebPointerProperties::PointerType::kMouse,
            GetPointerType(events));

  // Clean up.
  host->ShutdownAndDestroyWidget(true);
}

void RenderWidgetHostViewMacTest::
    IgnoreEmptyUnhandledWheelEventWithWheelGestures() {
  // Initialize the view associated with a MockRenderWidgetHostImpl, rather than
  // the MockRenderProcessHost that is set up by the test harness which mocks
  // out |OnMessageReceived()|.
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  process_host->Init();
  MockRenderWidgetHostDelegate delegate;
  int32_t routing_id = process_host->GetNextRoutingID();
  MockRenderWidgetHostImpl* host =
      MockRenderWidgetHostImpl::Create(&delegate, process_host, routing_id);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(host, false);
  base::RunLoop().RunUntilIdle();

  // Add a delegate to the view.
  base::scoped_nsobject<MockRenderWidgetHostViewMacDelegate> view_delegate(
      [[MockRenderWidgetHostViewMacDelegate alloc] init]);
  view->SetDelegate(view_delegate.get());

  // Send an initial wheel event for scrolling by 3 lines.
  NSEvent* event1 = MockScrollWheelEventWithPhase(@selector(phaseBegan), 3);
  [view->cocoa_view() scrollWheel:event1];
  base::RunLoop().RunUntilIdle();

  MockWidgetInputHandler::MessageVector events =
      host->GetAndResetDispatchedMessages();

  ASSERT_EQ("MouseWheel", GetMessageNames(events));

  // Indicate that the wheel event was unhandled.
  events.clear();

  base::RunLoop().RunUntilIdle();
  events = host->GetAndResetDispatchedMessages();

  if (scroll_latching_) {
    // GestureEventQueue allows multiple in-flight events.
    ASSERT_EQ("GestureScrollBegin GestureScrollUpdate",
              GetMessageNames(events));
    events[0]->ToEvent()->CallCallback(INPUT_EVENT_ACK_STATE_CONSUMED);
  } else {
    // GestureEventQueue allows multiple in-flight events.
    ASSERT_EQ("GestureScrollBegin GestureScrollUpdate GestureScrollEnd",
              GetMessageNames(events));
  }
  events.clear();

  // Check that the view delegate got an unhandled wheel event.
  ASSERT_EQ(YES, view_delegate.get().unhandledWheelEventReceived);
  view_delegate.get().unhandledWheelEventReceived = NO;

  // Send another wheel event, this time for scrolling by 0 lines (empty event).
  NSEvent* event2 = MockScrollWheelEventWithPhase(@selector(phaseChanged), 0);
  [view->cocoa_view() scrollWheel:event2];
  base::RunLoop().RunUntilIdle();
  events = host->GetAndResetDispatchedMessages();
  ASSERT_EQ("MouseWheel", GetMessageNames(events));

  events.clear();

  // Check that the view delegate ignored the empty unhandled wheel event.
  ASSERT_EQ(NO, view_delegate.get().unhandledWheelEventReceived);

  // Clean up.
  host->ShutdownAndDestroyWidget(true);
}
TEST_F(RenderWidgetHostViewMacTest,
       IgnoreEmptyUnhandledWheelEventWithWheelGestures) {
  IgnoreEmptyUnhandledWheelEventWithWheelGestures();
}

// Tests that when view initiated shutdown happens (i.e. RWHView is deleted
// before RWH), we clean up properly and don't leak the RWHVGuest.
TEST_F(RenderWidgetHostViewMacTest, GuestViewDoesNotLeak) {
  MockRenderWidgetHostDelegate delegate;
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  process_host->Init();
  int32_t routing_id = process_host->GetNextRoutingID();

  // Owned by its |cocoa_view()|.
  MockRenderWidgetHostImpl* rwh =
      MockRenderWidgetHostImpl::Create(&delegate, process_host, routing_id);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(rwh, true);

  // Add a delegate to the view.
  base::scoped_nsobject<MockRenderWidgetHostViewMacDelegate> view_delegate(
      [[MockRenderWidgetHostViewMacDelegate alloc] init]);
  view->SetDelegate(view_delegate.get());

  base::WeakPtr<RenderWidgetHostViewBase> guest_rwhv_weak =
      (RenderWidgetHostViewGuest::Create(rwh, nullptr, view->GetWeakPtr()))
          ->GetWeakPtr();

  // Remove the cocoa_view() so |view| also goes away before |rwh|.
  {
    base::scoped_nsobject<RenderWidgetHostViewCocoa> rwhv_cocoa;
    rwhv_cocoa.reset([view->cocoa_view() retain]);
  }
  RecycleAndWait();

  // Clean up.
  rwh->ShutdownAndDestroyWidget(true);

  // Let |guest_rwhv_weak| have a chance to delete itself.
  base::RunLoop run_loop;
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_FALSE(guest_rwhv_weak.get());
}

// Tests setting background transparency. See also (disabled on Mac)
// RenderWidgetHostTest.Background. This test has some additional checks for
// Mac.
TEST_F(RenderWidgetHostViewMacTest, Background) {
  const IPC::Message* set_background = nullptr;
  std::tuple<bool> sent_background;

  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  process_host->Init();
  MockRenderWidgetHostDelegate delegate;
  int32_t routing_id = process_host->GetNextRoutingID();
  MockRenderWidgetHostImpl* host =
      MockRenderWidgetHostImpl::Create(&delegate, process_host, routing_id);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(host, false);

  // If no color has been specified then default color of white should be
  // returned.
  EXPECT_EQ(static_cast<unsigned>(SK_ColorWHITE), view->background_color());

  // Set the color to red. The background is initially assumed to be opaque, so
  // no opacity message change should be sent.
  view->SetBackgroundColor(SK_ColorRED);
  EXPECT_EQ(static_cast<unsigned>(SK_ColorRED), view->background_color());
  set_background = process_host->sink().GetUniqueMessageMatching(
      ViewMsg_SetBackgroundOpaque::ID);
  ASSERT_FALSE(set_background);

  // Set the color to blue. This should not send an opacity message.
  view->SetBackgroundColor(SK_ColorBLUE);
  EXPECT_EQ(static_cast<unsigned>(SK_ColorBLUE), view->background_color());
  set_background = process_host->sink().GetUniqueMessageMatching(
      ViewMsg_SetBackgroundOpaque::ID);
  ASSERT_FALSE(set_background);

  // Set the color back to transparent. The background color should now be
  // reported as the default (white), and a transparency change message should
  // be sent.
  process_host->sink().ClearMessages();
  view->SetBackgroundColor(SK_ColorTRANSPARENT);
  EXPECT_EQ(static_cast<unsigned>(SK_ColorWHITE), view->background_color());
  set_background = process_host->sink().GetUniqueMessageMatching(
      ViewMsg_SetBackgroundOpaque::ID);
  ASSERT_TRUE(set_background);
  ViewMsg_SetBackgroundOpaque::Read(set_background, &sent_background);
  EXPECT_FALSE(std::get<0>(sent_background));

  // Set the color to red. This should send an opacity message.
  process_host->sink().ClearMessages();
  view->SetBackgroundColor(SK_ColorBLUE);
  EXPECT_EQ(static_cast<unsigned>(SK_ColorBLUE), view->background_color());
  set_background = process_host->sink().GetUniqueMessageMatching(
      ViewMsg_SetBackgroundOpaque::ID);
  ASSERT_TRUE(set_background);
  ViewMsg_SetBackgroundOpaque::Read(set_background, &sent_background);
  EXPECT_TRUE(std::get<0>(sent_background));

  host->ShutdownAndDestroyWidget(true);
}

class RenderWidgetHostViewMacWithWheelScrollLatchingEnabledTest
    : public RenderWidgetHostViewMacTest {
 public:
  RenderWidgetHostViewMacWithWheelScrollLatchingEnabledTest()
      : RenderWidgetHostViewMacTest(true) {}
};

TEST_F(RenderWidgetHostViewMacWithWheelScrollLatchingEnabledTest,
       IgnoreEmptyUnhandledWheelEventWithWheelGestures) {
  IgnoreEmptyUnhandledWheelEventWithWheelGestures();
}

TEST_F(RenderWidgetHostViewMacWithWheelScrollLatchingEnabledTest,
       ScrollWheelEndEventDelivery) {
  ScrollWheelEndEventDelivery();
}

// Scrolling with a mouse wheel device on Mac won't give phase information.
// MouseWheelPhaseHandler adds timer based phase information to wheel events
// generated from this type of devices.
TEST_F(RenderWidgetHostViewMacWithWheelScrollLatchingEnabledTest,
       TimerBasedPhaseInfo) {
  // Initialize the view associated with a MockRenderWidgetHostImpl, rather than
  // the MockRenderProcessHost that is set up by the test harness which mocks
  // out |OnMessageReceived()|.
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  process_host->Init();
  MockRenderWidgetHostDelegate delegate;
  int32_t routing_id = process_host->GetNextRoutingID();
  MockRenderWidgetHostImpl* host =
      MockRenderWidgetHostImpl::Create(&delegate, process_host, routing_id);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(host, false);
  base::RunLoop().RunUntilIdle();

  // Send a wheel event without phase information for scrolling by 3 lines.
  NSEvent* wheelEvent = MockScrollWheelEventWithoutPhase(3);
  [view->cocoa_view() scrollWheel:wheelEvent];
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      host->GetAndResetDispatchedMessages();
  ASSERT_EQ("MouseWheel", GetMessageNames(events));

  events.clear();
  events = host->GetAndResetDispatchedMessages();
  // Both GSB and GSU will be sent since GestureEventQueue allows multiple
  // in-flight events.
  ASSERT_EQ("GestureScrollBegin GestureScrollUpdate", GetMessageNames(events));
  ASSERT_TRUE(static_cast<const blink::WebGestureEvent*>(
                  events[0]->ToEvent()->Event()->web_event.get())
                  ->data.scroll_begin.synthetic);
  events.clear();

  // Wait for the mouse_wheel_end_dispatch_timer_ to expire, the pending wheel
  // event gets dispatched.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(100));
  run_loop.Run();

  events = host->GetAndResetDispatchedMessages();
  ASSERT_EQ("MouseWheel GestureScrollEnd", GetMessageNames(events));
  ASSERT_TRUE(static_cast<const blink::WebGestureEvent*>(
                  events[1]->ToEvent()->Event()->web_event.get())
                  ->data.scroll_end.synthetic);
  host->ShutdownAndDestroyWidget(true);
}

// When wheel scroll latching is enabled, wheel end events are not sent
// immediately, instead we start a timer to see if momentum phase of the scroll
// starts or not.
TEST_F(RenderWidgetHostViewMacWithWheelScrollLatchingEnabledTest,
       WheelWithPhaseEndedIsNotForwardedImmediately) {
  // Initialize the view associated with a MockRenderWidgetHostImpl, rather than
  // the MockRenderProcessHost that is set up by the test harness which mocks
  // out |OnMessageReceived()|.
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  process_host->Init();
  MockRenderWidgetHostDelegate delegate;
  int32_t routing_id = process_host->GetNextRoutingID();
  MockRenderWidgetHostImpl* host =
      MockRenderWidgetHostImpl::Create(&delegate, process_host, routing_id);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(host, false);
  base::RunLoop().RunUntilIdle();

  // Send an initial wheel event for scrolling by 3 lines.
  NSEvent* wheelEvent1 =
      MockScrollWheelEventWithPhase(@selector(phaseBegan), 3);
  [view->cocoa_view() scrollWheel:wheelEvent1];
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      host->GetAndResetDispatchedMessages();
  ASSERT_EQ("MouseWheel", GetMessageNames(events));

  events.clear();
  events = host->GetAndResetDispatchedMessages();
  // Both GSB and GSU will be sent since GestureEventQueue allows multiple
  // in-flight events.
  ASSERT_EQ("GestureScrollBegin GestureScrollUpdate", GetMessageNames(events));

  // Send a wheel event with phaseEnded. When wheel scroll latching is enabled
  // the event will be dropped and the mouse_wheel_end_dispatch_timer_ will
  // start.
  NSEvent* wheelEvent2 =
      MockScrollWheelEventWithPhase(@selector(phaseEnded), 0);
  [view->cocoa_view() scrollWheel:wheelEvent2];
  base::RunLoop().RunUntilIdle();
  events = host->GetAndResetDispatchedMessages();
  ASSERT_EQ(0U, events.size());
  DCHECK(view->HasPendingWheelEndEventForTesting());

  host->ShutdownAndDestroyWidget(true);

  // Wait for the mouse_wheel_end_dispatch_timer_ to expire after host is
  // destroyed. The pending wheel end event won't get dispatched since the
  // render_widget_host_ is null. This waiting confirms that no crash happens
  // because of an attempt to send the pending wheel end event.
  // https://crbug.com/770057
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(100));
  run_loop.Run();
}

TEST_F(RenderWidgetHostViewMacWithWheelScrollLatchingEnabledTest,
       WheelWithMomentumPhaseBeganStopsTheWheelEndDispatchTimer) {
  // Initialize the view associated with a MockRenderWidgetHostImpl, rather than
  // the MockRenderProcessHost that is set up by the test harness which mocks
  // out |OnMessageReceived()|.
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  process_host->Init();
  MockRenderWidgetHostDelegate delegate;
  int32_t routing_id = process_host->GetNextRoutingID();
  MockRenderWidgetHostImpl* host =
      MockRenderWidgetHostImpl::Create(&delegate, process_host, routing_id);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(host, false);
  base::RunLoop().RunUntilIdle();

  // Send an initial wheel event for scrolling by 3 lines.
  NSEvent* wheelEvent1 =
      MockScrollWheelEventWithPhase(@selector(phaseBegan), 3);
  [view->cocoa_view() scrollWheel:wheelEvent1];
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      host->GetAndResetDispatchedMessages();
  ASSERT_EQ("MouseWheel", GetMessageNames(events));

  // Indicate that the wheel event was unhandled.
  events.clear();
  events = host->GetAndResetDispatchedMessages();
  // Both GSB and GSU will be sent since GestureEventQueue allows multiple
  // in-flight events.
  ASSERT_EQ("GestureScrollBegin GestureScrollUpdate", GetMessageNames(events));

  // Send a wheel event with phaseEnded. When wheel scroll latching is enabled
  // the event will be dropped and the mouse_wheel_end_dispatch_timer_ will
  // start.
  NSEvent* wheelEvent2 =
      MockScrollWheelEventWithPhase(@selector(phaseEnded), 0);
  [view->cocoa_view() scrollWheel:wheelEvent2];
  base::RunLoop().RunUntilIdle();
  events = host->GetAndResetDispatchedMessages();
  ASSERT_EQ(0U, events.size());
  DCHECK(view->HasPendingWheelEndEventForTesting());

  // Send a wheel event with momentum phase started, this should stop the wheel
  // end dispatch timer.
  NSEvent* wheelEvent3 =
      MockScrollWheelEventWithMomentumPhase(@selector(phaseBegan), 3);
  ASSERT_TRUE(wheelEvent3);
  [view->cocoa_view() scrollWheel:wheelEvent3];
  base::RunLoop().RunUntilIdle();
  events = host->GetAndResetDispatchedMessages();
  ASSERT_EQ("MouseWheel", GetMessageNames(events));
  DCHECK(!view->HasPendingWheelEndEventForTesting());

  host->ShutdownAndDestroyWidget(true);
}

TEST_F(RenderWidgetHostViewMacWithWheelScrollLatchingEnabledTest,
       WheelWithPhaseBeganDispatchesThePendingWheelEnd) {
  // Initialize the view associated with a MockRenderWidgetHostImpl, rather than
  // the MockRenderProcessHost that is set up by the test harness which mocks
  // out |OnMessageReceived()|.
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  process_host->Init();
  MockRenderWidgetHostDelegate delegate;
  int32_t routing_id = process_host->GetNextRoutingID();
  MockRenderWidgetHostImpl* host =
      MockRenderWidgetHostImpl::Create(&delegate, process_host, routing_id);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(host, false);
  base::RunLoop().RunUntilIdle();

  // Send an initial wheel event for scrolling by 3 lines.
  NSEvent* wheelEvent1 =
      MockScrollWheelEventWithPhase(@selector(phaseBegan), 3);
  [view->cocoa_view() scrollWheel:wheelEvent1];
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      host->GetAndResetDispatchedMessages();
  ASSERT_EQ("MouseWheel", GetMessageNames(events));

  // Indicate that the wheel event was unhandled.
  events.clear();
  // Both GSB and GSU will be sent since GestureEventQueue allows multiple
  // in-flight events.
  events = host->GetAndResetDispatchedMessages();
  ASSERT_EQ("GestureScrollBegin GestureScrollUpdate", GetMessageNames(events));

  // Send a wheel event with phaseEnded. When wheel scroll latching is enabled
  // the event will be dropped and the mouse_wheel_end_dispatch_timer_ will
  // start.
  NSEvent* wheelEvent2 =
      MockScrollWheelEventWithPhase(@selector(phaseEnded), 0);
  [view->cocoa_view() scrollWheel:wheelEvent2];
  base::RunLoop().RunUntilIdle();
  events = host->GetAndResetDispatchedMessages();
  ASSERT_EQ(0U, events.size());
  DCHECK(view->HasPendingWheelEndEventForTesting());

  // Send a wheel event with phase started, this should stop the wheel end
  // dispatch timer and dispatch the pending wheel end event for the previous
  // scroll sequence.
  NSEvent* wheelEvent3 =
      MockScrollWheelEventWithPhase(@selector(phaseBegan), 3);
  ASSERT_TRUE(wheelEvent3);
  [view->cocoa_view() scrollWheel:wheelEvent3];
  base::RunLoop().RunUntilIdle();
  events = host->GetAndResetDispatchedMessages();
  ASSERT_EQ("MouseWheel GestureScrollEnd MouseWheel", GetMessageNames(events));
  DCHECK(!view->HasPendingWheelEndEventForTesting());

  host->ShutdownAndDestroyWidget(true);
}

class RenderWidgetHostViewMacPinchTest : public RenderWidgetHostViewMacTest {
 public:
  RenderWidgetHostViewMacPinchTest() = default;

  void SetUp() override {
    RenderWidgetHostViewMacTest::SetUp();
    // Initialize the view associated with a MockRenderWidgetHostImpl, rather
    // than the MockRenderProcessHost that is set up by the test harness which
    // mocks out |OnMessageReceived()|.
    browser_context_.reset(new TestBrowserContext);
    process_host_.reset(new MockRenderProcessHost(browser_context_.get()));
    process_host_->Init();
    delegate_.reset(new MockRenderWidgetHostDelegate);
    int32_t routing_id = process_host_->GetNextRoutingID();
    host_.reset(MockRenderWidgetHostImpl::Create(
        delegate_.get(), process_host_.get(), routing_id));
    view_ = new RenderWidgetHostViewMac(host_.get(), false);
    cocoa_view_.reset([view_->cocoa_view() retain]);
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    cocoa_view_.reset();
    host_->ShutdownAndDestroyWidget(false);
    host_.reset();
    delegate_.reset();
    process_host_.reset();
    browser_context_.reset();
    RenderWidgetHostViewMacTest::TearDown();
  }

  bool ZoomDisabledForPinchUpdateMessage(
      const MockWidgetInputHandler::MessageVector& events) {
    MockWidgetInputHandler::DispatchedEventMessage* event =
        events[events.size() - 1]->ToEvent();
    EXPECT_TRUE(event);

    return static_cast<const blink::WebGestureEvent*>(
               event->Event()->web_event.get())
        ->data.pinch_update.zoom_disabled;
  }

  bool ShouldSendGestureEvents() {
#if defined(MAC_OS_X_VERSION_10_11) && \
    MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_11
    return base::mac::IsAtMostOS10_10();
#endif
    return true;
  }

  void SendBeginEvent() {
    NSEvent* pinchBeginEvent = MockGestureEvent(NSEventTypeBeginGesture, 0);
    if (ShouldSendGestureEvents())
      [cocoa_view_ beginGestureWithEvent:pinchBeginEvent];
    [cocoa_view_ magnifyWithEvent:pinchBeginEvent];
  }

  void SendEndEvent() {
    NSEvent* pinchEndEvent = MockGestureEvent(NSEventTypeEndGesture, 0);
    [cocoa_view_ magnifyWithEvent:pinchEndEvent];
    if (ShouldSendGestureEvents())
      [cocoa_view_ endGestureWithEvent:pinchEndEvent];
  }

  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<MockRenderProcessHost> process_host_;
  std::unique_ptr<MockRenderWidgetHostImpl> host_;
  std::unique_ptr<MockRenderWidgetHostDelegate> delegate_;

  // Owned by view_->cocoa_view(), which is stored in cocoa_view_.
  RenderWidgetHostViewMac* view_;
  base::scoped_nsobject<RenderWidgetHostViewCocoa> cocoa_view_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewMacPinchTest);
};

TEST_F(RenderWidgetHostViewMacPinchTest, PinchThresholding) {
  // Do a gesture that crosses the threshold.
  {
    NSEvent* pinchUpdateEvents[3] = {
        MockGestureEvent(NSEventTypeMagnify, 0.25),
        MockGestureEvent(NSEventTypeMagnify, 0.25),
        MockGestureEvent(NSEventTypeMagnify, 0.25),
    };

    SendBeginEvent();
    base::RunLoop().RunUntilIdle();
    MockWidgetInputHandler::MessageVector events =
        host_->GetAndResetDispatchedMessages();

    EXPECT_EQ(0U, events.size());

    // No zoom is sent for the first update event.
    [cocoa_view_ magnifyWithEvent:pinchUpdateEvents[0]];
    base::RunLoop().RunUntilIdle();
    events = host_->GetAndResetDispatchedMessages();
    EXPECT_EQ("GesturePinchBegin GesturePinchUpdate", GetMessageNames(events));
    EXPECT_TRUE(ZoomDisabledForPinchUpdateMessage(events));

    // The second update event crosses the threshold of 0.4, and so zoom is no
    // longer disabled.
    [cocoa_view_ magnifyWithEvent:pinchUpdateEvents[1]];
    base::RunLoop().RunUntilIdle();
    events = host_->GetAndResetDispatchedMessages();
    EXPECT_EQ("GesturePinchUpdate", GetMessageNames(events));
    EXPECT_FALSE(ZoomDisabledForPinchUpdateMessage(events));

    // The third update still has zoom enabled.
    [cocoa_view_ magnifyWithEvent:pinchUpdateEvents[2]];
    base::RunLoop().RunUntilIdle();
    events = host_->GetAndResetDispatchedMessages();
    EXPECT_EQ("GesturePinchUpdate", GetMessageNames(events));
    EXPECT_FALSE(ZoomDisabledForPinchUpdateMessage(events));

    SendEndEvent();
    base::RunLoop().RunUntilIdle();
    events = host_->GetAndResetDispatchedMessages();
    EXPECT_EQ("GesturePinchEnd", GetMessageNames(events));
  }

  // Do a gesture that doesn't cross the threshold, but happens when we're not
  // at page scale factor one, so it should be sent to the renderer.
  {
    NSEvent* pinchUpdateEvent = MockGestureEvent(NSEventTypeMagnify, 0.25);

    view_->page_at_minimum_scale_ = false;

    SendBeginEvent();
    base::RunLoop().RunUntilIdle();
    MockWidgetInputHandler::MessageVector events =
        host_->GetAndResetDispatchedMessages();
    EXPECT_EQ(0U, events.size());

    // Expect that a zoom happen because the time threshold has not passed.
    [cocoa_view_ magnifyWithEvent:pinchUpdateEvent];
    base::RunLoop().RunUntilIdle();
    events = host_->GetAndResetDispatchedMessages();
    EXPECT_EQ("GesturePinchBegin GesturePinchUpdate", GetMessageNames(events));
    EXPECT_FALSE(ZoomDisabledForPinchUpdateMessage(events));

    SendEndEvent();
    base::RunLoop().RunUntilIdle();
    events = host_->GetAndResetDispatchedMessages();
    EXPECT_EQ("GesturePinchEnd", GetMessageNames(events));
  }

  // Do a gesture again, after the page scale is no longer at one, and ensure
  // that it is thresholded again.
  {
    NSEvent* pinchUpdateEvent = MockGestureEvent(NSEventTypeMagnify, 0.25);

    view_->page_at_minimum_scale_ = true;

    SendBeginEvent();
    base::RunLoop().RunUntilIdle();
    MockWidgetInputHandler::MessageVector events =
        host_->GetAndResetDispatchedMessages();
    EXPECT_EQ(0U, events.size());

    // Get back to zoom one right after the begin event. This should still keep
    // the thresholding in place (it is latched at the begin event).
    view_->page_at_minimum_scale_ = false;

    // Expect that zoom be disabled because the time threshold has passed.
    [cocoa_view_ magnifyWithEvent:pinchUpdateEvent];
    base::RunLoop().RunUntilIdle();
    events = host_->GetAndResetDispatchedMessages();
    EXPECT_EQ("GesturePinchBegin GesturePinchUpdate", GetMessageNames(events));
    EXPECT_TRUE(ZoomDisabledForPinchUpdateMessage(events));

    SendEndEvent();
    base::RunLoop().RunUntilIdle();
    events = host_->GetAndResetDispatchedMessages();
    EXPECT_EQ("GesturePinchEnd", GetMessageNames(events));
  }
}

TEST_F(RenderWidgetHostViewMacTest, EventLatencyOSMouseWheelHistogram) {
  base::HistogramTester histogram_tester;

  // Initialize the view associated with a MockRenderWidgetHostImpl, rather than
  // the MockRenderProcessHost that is set up by the test harness which mocks
  // out |OnMessageReceived()|.
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  process_host->Init();
  MockRenderWidgetHostDelegate delegate;
  int32_t routing_id = process_host->GetNextRoutingID();
  MockRenderWidgetHostImpl* host =
      MockRenderWidgetHostImpl::Create(&delegate, process_host, routing_id);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(host, false);
  process_host->sink().ClearMessages();

  // Send an initial wheel event for scrolling by 3 lines.
  // Verify that Event.Latency.OS.MOUSE_WHEEL histogram is computed properly.
  NSEvent* wheelEvent = MockScrollWheelEventWithPhase(@selector(phaseBegan),3);
  [view->cocoa_view() scrollWheel:wheelEvent];
  histogram_tester.ExpectTotalCount("Event.Latency.OS.MOUSE_WHEEL", 1);

  // Clean up.
  host->ShutdownAndDestroyWidget(true);
}

// This test verifies that |selected_text_| is updated accordingly with
// different variations of RWHVMac::SelectChanged updates.
TEST_F(RenderWidgetHostViewMacTest, SelectedText) {
  base::string16 sample_text;
  base::UTF8ToUTF16("hello world!", 12, &sample_text);
  gfx::Range range(6, 11);

  // Send a valid selection for the word 'World'.
  rwhv_mac_->SelectionChanged(sample_text, 0U, range);
  EXPECT_EQ("world", selected_text());

  // Make the range cover some of the text and extend more.
  range.set_end(100);
  rwhv_mac_->SelectionChanged(sample_text, 0U, range);
  EXPECT_EQ("world!", selected_text());

  // Finally, send an empty range. This should clear the selected text.
  range.set_start(100);
  rwhv_mac_->SelectionChanged(sample_text, 0U, range);
  EXPECT_EQ("", selected_text());
}

// This class is used for IME-related unit tests which verify correctness of IME
// for pages with multiple RWHVs.
class InputMethodMacTest : public RenderWidgetHostViewMacTest {
 public:
  InputMethodMacTest() {}
  ~InputMethodMacTest() override {}

  void SetUp() override {
    RenderWidgetHostViewMacTest::SetUp();
    process_host_ = new MockRenderProcessHost(&browser_context_);
    process_host_->Init();
    widget_ = MockRenderWidgetHostImpl::Create(
        &delegate_, process_host_, process_host_->GetNextRoutingID());
    view_ = new RenderWidgetHostViewMac(widget_, false);

    // Initializing a child frame's view.
    child_process_host_ = new MockRenderProcessHost(&child_browser_context_);
    child_process_host_->Init();
    child_widget_ = MockRenderWidgetHostImpl::Create(
        &delegate_, child_process_host_,
        child_process_host_->GetNextRoutingID());
    child_view_ = new TestRenderWidgetHostView(child_widget_);
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    widget_->ShutdownAndDestroyWidget(true);
    child_widget_->ShutdownAndDestroyWidget(true);

    RenderWidgetHostViewMacTest::TearDown();
  }

  void SetTextInputType(RenderWidgetHostViewBase* view,
                        ui::TextInputType type) {
    TextInputState state;
    state.type = type;
    view->TextInputStateChanged(state);
  }

  IPC::TestSink& tab_sink() { return process()->sink(); }
  IPC::TestSink& child_sink() { return child_process_host_->sink(); }
  TextInputManager* text_input_manager() {
    return delegate_.GetTextInputManager();
  }
  RenderWidgetHostViewBase* tab_view() { return view_; }
  RenderWidgetHostImpl* tab_widget() { return widget_; }
  RenderWidgetHostViewCocoa* tab_cocoa_view() { return view_->cocoa_view(); }

 protected:
  MockRenderProcessHost* process_host_;
  MockRenderWidgetHostImpl* widget_;
  MockRenderWidgetHostDelegate delegate_;
  RenderWidgetHostViewMac* view_;

  MockRenderProcessHost* child_process_host_;
  MockRenderWidgetHostImpl* child_widget_;
  TestRenderWidgetHostView* child_view_;

 private:
  TestBrowserContext browser_context_;
  TestBrowserContext child_browser_context_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodMacTest);
};

// This test will verify that calling unmarkText on the cocoa view will lead to
// a finish composing text IPC for the corresponding active widget.
TEST_F(InputMethodMacTest, UnmarkText) {
  // Make the child view active and then call unmarkText on the view (Note that
  // |RenderWidgetHostViewCocoa::handlingKeyDown_| is false so calling
  // unmarkText would lead to an IPC. This assumption is made in other similar
  // tests as well). We should observe an IPC being sent to the |child_widget_|.
  SetTextInputType(child_view_, ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(child_widget_, text_input_manager()->GetActiveWidget());
  [tab_cocoa_view() unmarkText];
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      child_widget_->GetAndResetDispatchedMessages();
  EXPECT_EQ("FinishComposingText", GetMessageNames(events));

  // Repeat the same steps for the tab's view .
  SetTextInputType(tab_view(), ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(tab_widget(), text_input_manager()->GetActiveWidget());
  [tab_cocoa_view() unmarkText];
  base::RunLoop().RunUntilIdle();
  events = widget_->GetAndResetDispatchedMessages();
  EXPECT_EQ("FinishComposingText", GetMessageNames(events));
}

// This test makes sure that calling setMarkedText on the cocoa view will lead
// to a set composition IPC for the corresponding active widget.
TEST_F(InputMethodMacTest, SetMarkedText) {
  // Some values for the call to setMarkedText.
  base::scoped_nsobject<NSString> text(
      [[NSString alloc] initWithString:@"sample text"]);
  NSRange selectedRange = NSMakeRange(0, 4);
  NSRange replacementRange = NSMakeRange(0, 1);

  // Make the child view active and then call setMarkedText with some values. We
  // should observe an IPC being sent to the |child_widget_|.
  SetTextInputType(child_view_, ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(child_widget_, text_input_manager()->GetActiveWidget());
  [tab_cocoa_view() setMarkedText:text
                    selectedRange:selectedRange
                 replacementRange:replacementRange];
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      child_widget_->GetAndResetDispatchedMessages();
  EXPECT_EQ("SetComposition", GetMessageNames(events));

  // Repeat the same steps for the tab's view.
  SetTextInputType(tab_view(), ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(tab_widget(), text_input_manager()->GetActiveWidget());
  [tab_cocoa_view() setMarkedText:text
                    selectedRange:selectedRange
                 replacementRange:replacementRange];
  base::RunLoop().RunUntilIdle();
  events = widget_->GetAndResetDispatchedMessages();
  EXPECT_EQ("SetComposition", GetMessageNames(events));
}

// This test verifies that calling insertText on the cocoa view will lead to a
// commit text IPC sent to the active widget.
TEST_F(InputMethodMacTest, InsertText) {
  // Some values for the call to insertText.
  base::scoped_nsobject<NSString> text(
      [[NSString alloc] initWithString:@"sample text"]);
  NSRange replacementRange = NSMakeRange(0, 1);

  // Make the child view active and then call insertText with some values. We
  // should observe an IPC being sent to the |child_widget_|.
  SetTextInputType(child_view_, ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(child_widget_, text_input_manager()->GetActiveWidget());
  [tab_cocoa_view() insertText:text replacementRange:replacementRange];
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      child_widget_->GetAndResetDispatchedMessages();
  EXPECT_EQ("CommitText", GetMessageNames(events));

  // Repeat the same steps for the tab's view.
  SetTextInputType(tab_view(), ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(tab_widget(), text_input_manager()->GetActiveWidget());
  [tab_cocoa_view() insertText:text replacementRange:replacementRange];
  base::RunLoop().RunUntilIdle();
  events = widget_->GetAndResetDispatchedMessages();
  EXPECT_EQ("CommitText", GetMessageNames(events));
}

// This test makes sure that calling finishComposingText on the cocoa view will
// lead to a finish composing text IPC for a the corresponding active widget.
TEST_F(InputMethodMacTest, FinishComposingText) {
  // Some values for the call to setMarkedText.
  base::scoped_nsobject<NSString> text(
      [[NSString alloc] initWithString:@"sample text"]);
  NSRange selectedRange = NSMakeRange(0, 4);
  NSRange replacementRange = NSMakeRange(0, 1);

  // Make child view active and then call finishComposingText. We should observe
  // an IPC being sent to the |child_widget_|.
  SetTextInputType(child_view_, ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(child_widget_, text_input_manager()->GetActiveWidget());
  // In order to finish composing text, we must first have some marked text. So,
  // we will first call setMarkedText on cocoa view. This would lead to a set
  // composition IPC in the sink, but it doesn't matter since we will be looking
  // for a finish composing text IPC for this test.
  [tab_cocoa_view() setMarkedText:text
                    selectedRange:selectedRange
                 replacementRange:replacementRange];
  [tab_cocoa_view() finishComposingText];
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      child_widget_->GetAndResetDispatchedMessages();
  EXPECT_EQ("SetComposition FinishComposingText", GetMessageNames(events));

  // Repeat the same steps for the tab's view.
  SetTextInputType(tab_view(), ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(tab_widget(), text_input_manager()->GetActiveWidget());
  [tab_cocoa_view() setMarkedText:text
                    selectedRange:selectedRange
                 replacementRange:replacementRange];
  [tab_cocoa_view() finishComposingText];
  base::RunLoop().RunUntilIdle();
  events = widget_->GetAndResetDispatchedMessages();
  EXPECT_EQ("SetComposition FinishComposingText", GetMessageNames(events));
}

// This test creates a test view to mimic a child frame's view and verifies that
// calling ImeCancelComposition on either the child view or the tab's view will
// always lead to a call to cancelComposition on the cocoa view.
TEST_F(InputMethodMacTest, ImeCancelCompositionForAllViews) {
  // Some values for the call to setMarkedText.
  base::scoped_nsobject<NSString> text(
      [[NSString alloc] initWithString:@"sample text"]);
  NSRange selectedRange = NSMakeRange(0, 1);
  NSRange replacementRange = NSMakeRange(0, 1);

  // Make Cocoa view assume there is marked text.
  [tab_cocoa_view() setMarkedText:text
                    selectedRange:selectedRange
                 replacementRange:replacementRange];
  EXPECT_TRUE([tab_cocoa_view() hasMarkedText]);
  child_view_->ImeCancelComposition();
  EXPECT_FALSE([tab_cocoa_view() hasMarkedText]);

  // Repeat for the tab's view.
  [tab_cocoa_view() setMarkedText:text
                    selectedRange:selectedRange
                 replacementRange:replacementRange];
  EXPECT_TRUE([tab_cocoa_view() hasMarkedText]);
  tab_view()->ImeCancelComposition();
  EXPECT_FALSE([tab_cocoa_view() hasMarkedText]);
}

// This test verifies that calling FocusedNodeChanged() on
// RenderWidgetHostViewMac calls cancelComposition on the Cocoa view.
TEST_F(InputMethodMacTest, FocusedNodeChanged) {
  // Some values for the call to setMarkedText.
  base::scoped_nsobject<NSString> text(
      [[NSString alloc] initWithString:@"sample text"]);
  NSRange selectedRange = NSMakeRange(0, 1);
  NSRange replacementRange = NSMakeRange(0, 1);

  [tab_cocoa_view() setMarkedText:text
                    selectedRange:selectedRange
                 replacementRange:replacementRange];
  EXPECT_TRUE([tab_cocoa_view() hasMarkedText]);
  tab_view()->FocusedNodeChanged(true, gfx::Rect());
  EXPECT_FALSE([tab_cocoa_view() hasMarkedText]);
}

// This test verifies that when a RenderWidgetHostView changes its
// TextInputState to NONE we send the IPC to stop monitor composition info and,
// conversely, when its state is set to non-NONE, we start monitoring the
// composition info.
TEST_F(InputMethodMacTest, MonitorCompositionRangeForActiveWidget) {
  // First, we need to make the cocoa view the first responder so that the
  // method RWHVMac::HasFocus() returns true. Then we can make sure that as long
  // as there is some TextInputState of non-NONE, the corresponding widget will
  // be asked to start monitoring composition info.
  base::scoped_nsobject<CocoaTestHelperWindow> window(
      [[CocoaTestHelperWindow alloc] init]);
  [[window contentView] addSubview:tab_cocoa_view()];
  [window makeFirstResponder:tab_cocoa_view()];
  EXPECT_TRUE(view_->HasFocus());

  TextInputState state;
  state.type = ui::TEXT_INPUT_TYPE_TEXT;

  // Make the tab's widget active.
  view_->TextInputStateChanged(state);

  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      widget_->GetAndResetDispatchedMessages();
  // The tab's widget must have received an IPC regarding composition updates.
  EXPECT_EQ("SetFocus RequestCompositionUpdates", GetMessageNames(events));

  // The message should ask for monitoring updates, but no immediate update.
  MockWidgetInputHandler::DispatchedRequestCompositionUpdatesMessage* message =
      events.at(1)->ToRequestCompositionUpdates();
  EXPECT_FALSE(message->immediate_request());
  EXPECT_TRUE(message->monitor_request());

  // Now make the child view active.
  child_view_->TextInputStateChanged(state);

  // The tab should receive another IPC for composition updates.
  base::RunLoop().RunUntilIdle();
  events = widget_->GetAndResetDispatchedMessages();
  // The tab's widget must have received an IPC regarding composition updates.
  EXPECT_EQ("RequestCompositionUpdates", GetMessageNames(events));
  // This time, the tab should have been asked to stop monitoring (and no
  // immediate updates).
  message = events.at(0)->ToRequestCompositionUpdates();
  EXPECT_FALSE(message->immediate_request());
  EXPECT_FALSE(message->monitor_request());

  // The child too must have received an IPC for composition updates.
  events = child_widget_->GetAndResetDispatchedMessages();
  EXPECT_EQ("RequestCompositionUpdates", GetMessageNames(events));

  // Verify that the message is asking for monitoring to start; but no immediate
  // updates.
  message = events.at(0)->ToRequestCompositionUpdates();
  EXPECT_FALSE(message->immediate_request());
  EXPECT_TRUE(message->monitor_request());

  // Make the tab view active again.
  view_->TextInputStateChanged(state);

  base::RunLoop().RunUntilIdle();
  events = child_widget_->GetAndResetDispatchedMessages();

  // Verify that the child received another IPC for composition updates.
  EXPECT_EQ("RequestCompositionUpdates", GetMessageNames(events));

  // Verify that this IPC is asking for no monitoring or immediate updates.
  message = events.at(0)->ToRequestCompositionUpdates();
  EXPECT_FALSE(message->immediate_request());
  EXPECT_FALSE(message->monitor_request());
}

// Ensure RenderWidgetHostViewMac claims hotkeys when AppKit spams the UI with
// -performKeyEquivalent:, but only when the window is key.
TEST_F(RenderWidgetHostViewMacTest, ForwardKeyEquivalentsOnlyIfKey) {
  MockRenderWidgetHostDelegate delegate;
  TestBrowserContext browser_context;

  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  process_host->Init();

  int32_t routing_id = process_host->GetNextRoutingID();
  // Owned by its |cocoa_view()|.
  MockRenderWidgetHostImpl* host =
      MockRenderWidgetHostImpl::Create(&delegate, process_host, routing_id);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(host, false);

  EXPECT_CALL(*host, Focus()).Times(2);
  EXPECT_CALL(*host, Blur());

  // This test needs an NSWindow. |rwhv_cocoa_| isn't in one, but going
  // fullscreen conveniently puts it in one.
  EXPECT_FALSE([view->cocoa_view() window]);
  view->InitAsFullscreen(nullptr);
  NSWindow* window = [view->cocoa_view() window];
  EXPECT_TRUE(window);
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      host->GetAndResetDispatchedMessages();

  ui::test::ScopedFakeNSWindowFocus key_window_faker;
  EXPECT_FALSE([window isKeyWindow]);
  base::RunLoop().RunUntilIdle();
  events = host->GetAndResetDispatchedMessages();

  EXPECT_EQ(0U, events.size());

  // Cmd+x.
  NSEvent* key_down =
      cocoa_test_event_utils::KeyEventWithType(NSKeyDown, NSCommandKeyMask);

  // Sending while not key should forward along the responder chain (e.g. to the
  // mainMenu). Note the event is being sent to the NSWindow, which may also ask
  // other parts of the UI to handle it, but in the test they should all say
  // "NO" as well.
  EXPECT_FALSE([window performKeyEquivalent:key_down]);
  base::RunLoop().RunUntilIdle();
  events = host->GetAndResetDispatchedMessages();
  EXPECT_EQ(0U, events.size());

  // Make key and send again. Event should be seen.
  [window makeKeyWindow];
  EXPECT_TRUE([window isKeyWindow]);
  base::RunLoop().RunUntilIdle();
  events = host->GetAndResetDispatchedMessages();

  // -performKeyEquivalent: now returns YES to prevent further propagation, and
  // the event is sent to the renderer.
  EXPECT_TRUE([window performKeyEquivalent:key_down]);
  base::RunLoop().RunUntilIdle();
  events = host->GetAndResetDispatchedMessages();
  EXPECT_EQ("RawKeyDown Char", GetMessageNames(events));

  view->release_pepper_fullscreen_window_for_testing();

  // Clean up.
  host->ShutdownAndDestroyWidget(true);
}

TEST_F(RenderWidgetHostViewMacTest, ClearCompositorFrame) {
  BrowserCompositorMac* browser_compositor =
      rwhv_mac_->BrowserCompositorForTesting();
  EXPECT_NE(browser_compositor->CompositorForTesting(), nullptr);
  EXPECT_TRUE(browser_compositor->CompositorForTesting()->IsLocked());
  rwhv_mac_->ClearCompositorFrame();
  EXPECT_NE(browser_compositor->CompositorForTesting(), nullptr);
  EXPECT_FALSE(browser_compositor->CompositorForTesting()->IsLocked());
}

}  // namespace content
