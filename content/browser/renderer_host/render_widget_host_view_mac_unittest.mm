// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_mac.h"

#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/utf_string_conversions.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/cocoa_test_event_utils.h"
#import "ui/base/test/ui_cocoa_test_helper.h"
#include "webkit/plugins/npapi/webplugin.h"

// Declare things that are part of the 10.7 SDK.
#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7
enum {
  NSEventPhaseNone        = 0, // event not associated with a phase.
  NSEventPhaseBegan       = 0x1 << 0,
  NSEventPhaseStationary  = 0x1 << 1,
  NSEventPhaseChanged     = 0x1 << 2,
  NSEventPhaseEnded       = 0x1 << 3,
  NSEventPhaseCancelled   = 0x1 << 4,
};
typedef NSUInteger NSEventPhase;

@interface NSEvent (LionAPI)
- (NSEventPhase)phase;
@end

#endif  // 10.7

// Helper class with methods used to mock -[NSEvent phase], used by
// |MockScrollWheelEventWithPhase()|.
@interface MockPhaseMethods : NSObject {
}

- (NSEventPhase)phaseBegan;
- (NSEventPhase)phaseEnded;
@end

@implementation MockPhaseMethods

- (NSEventPhase)phaseBegan {
  return NSEventPhaseBegan;
}
- (NSEventPhase)phaseEnded {
  return NSEventPhaseEnded;
}

@end

namespace content {

namespace {

class MockRenderWidgetHostDelegate : public RenderWidgetHostDelegate {
 public:
  MockRenderWidgetHostDelegate() {}
  virtual ~MockRenderWidgetHostDelegate() {}
};

class MockRenderWidgetHostImpl : public RenderWidgetHostImpl {
 public:
  MockRenderWidgetHostImpl(RenderWidgetHostDelegate* delegate,
                           RenderProcessHost* process,
                           int routing_id)
      : RenderWidgetHostImpl(delegate, process, routing_id) {
  }

  MOCK_METHOD0(Focus, void());
  MOCK_METHOD0(Blur, void());
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

  std::queue<int> break_point_queue;
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
                          const ui::Range& range,
                          int line_no) {
  return gfx::Rect(
      origin.x() + range.start() * size.width(),
      origin.y() + line_no * size.height(),
      range.length() * size.width(),
      size.height());
}

// Returns NSScrollWheel event that mocks -phase. |mockPhaseSelector| should
// correspond to a method in |MockPhaseMethods| that returns the desired phase.
NSEvent* MockScrollWheelEventWithPhase(SEL mockPhaseSelector) {
  CGEventRef cg_event =
      CGEventCreateScrollWheelEvent(NULL, kCGScrollEventUnitLine, 1, 0, 0);
  NSEvent* event = [NSEvent eventWithCGEvent:cg_event];
  CFRelease(cg_event);
  method_setImplementation(
      class_getInstanceMethod([NSEvent class], @selector(phase)),
      [MockPhaseMethods instanceMethodForSelector:mockPhaseSelector]);
  return event;
}

}  // namespace

class RenderWidgetHostViewMacTest : public RenderViewHostImplTestHarness {
 public:
  RenderWidgetHostViewMacTest() : old_rwhv_(NULL), rwhv_mac_(NULL) {}

  virtual void SetUp() {
    RenderViewHostImplTestHarness::SetUp();

    // TestRenderViewHost's destruction assumes that its view is a
    // TestRenderWidgetHostView, so store its view and reset it back to the
    // stored view in |TearDown()|.
    old_rwhv_ = rvh()->GetView();

    // Owned by its |cocoa_view()|, i.e. |rwhv_cocoa_|.
    rwhv_mac_ = static_cast<RenderWidgetHostViewMac*>(
        RenderWidgetHostView::CreateViewForWidget(rvh()));
    rwhv_cocoa_.reset([rwhv_mac_->cocoa_view() retain]);
  }
  virtual void TearDown() {
    // Make sure the rwhv_mac_ is gone once the superclass's |TearDown()| runs.
    rwhv_cocoa_.reset();
    pool_.Recycle();
    MessageLoop::current()->RunUntilIdle();
    pool_.Recycle();

    // See comment in SetUp().
    test_rvh()->SetView(old_rwhv_);

    RenderViewHostImplTestHarness::TearDown();
  }
 protected:
  // Adds an accelerated plugin view to |rwhv_cocoa_|.  Returns a handle to the
  // newly-added view.  Callers must ensure that a UI thread is present and
  // running before calling this function.
  gfx::PluginWindowHandle AddAcceleratedPluginView(int w, int h) {
    // Create an accelerated view the size of the rhwvmac.
    [rwhv_cocoa_.get() setFrame:NSMakeRect(0, 0, w, h)];
    gfx::PluginWindowHandle accelerated_handle;
    rwhv_mac_->OnAllocateFakePluginWindowHandle(/*opaque=*/false,
                                                /*root=*/false,
                                                &accelerated_handle);
    rwhv_mac_->OnAcceleratedSurfaceSetIOSurface(accelerated_handle, w, h, 0);

    // The accelerated view isn't shown until it has a valid rect and has been
    // painted to.
    GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
    params.window = accelerated_handle;
    rwhv_mac_->AcceleratedSurfaceBuffersSwapped(params, 0);
    webkit::npapi::WebPluginGeometry geom;
    gfx::Rect rect(0, 0, w, h);
    geom.window = accelerated_handle;
    geom.window_rect = rect;
    geom.clip_rect = rect;
    geom.visible = true;
    geom.rects_valid = true;
    rwhv_mac_->MovePluginWindows(
        gfx::Vector2d(),
        std::vector<webkit::npapi::WebPluginGeometry>(1, geom));

    return accelerated_handle;
  }
  
  void DestroyPluginView(gfx::PluginWindowHandle accelerated_handle) {
    rwhv_mac_->OnDestroyFakePluginWindowHandle(accelerated_handle);
  }
  
 private:
  // This class isn't derived from PlatformTest.
  base::mac::ScopedNSAutoreleasePool pool_;

  RenderWidgetHostView* old_rwhv_;

 protected:
  RenderWidgetHostViewMac* rwhv_mac_;
  scoped_nsobject<RenderWidgetHostViewCocoa> rwhv_cocoa_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewMacTest);
};

TEST_F(RenderWidgetHostViewMacTest, Basic) {
}

// Regression test for http://crbug.com/60318
TEST_F(RenderWidgetHostViewMacTest, FocusAcceleratedView) {
  // The accelerated view methods want to be called on the UI thread.
  scoped_ptr<BrowserThreadImpl> ui_thread_(
      new BrowserThreadImpl(BrowserThread::UI,
                            MessageLoop::current()));

  int w = 400, h = 300;
  gfx::PluginWindowHandle accelerated_handle = AddAcceleratedPluginView(w, h);
  EXPECT_FALSE([rwhv_cocoa_.get() isHidden]);
  NSView* accelerated_view = static_cast<NSView*>(
      rwhv_mac_->ViewForPluginWindowHandle(accelerated_handle));
  EXPECT_FALSE([accelerated_view isHidden]);

  // Take away first responder from the rwhvmac, then simulate the effect of a
  // click on the accelerated view. The rwhvmac should be first responder
  // again.
  scoped_nsobject<NSWindow> window([[CocoaTestHelperWindow alloc] init]);
  scoped_nsobject<NSView> other_view(
      [[NSTextField alloc] initWithFrame:NSMakeRect(0, h, w, 40)]);
  [[window contentView] addSubview:rwhv_cocoa_.get()];
  [[window contentView] addSubview:other_view.get()];

  EXPECT_TRUE([rwhv_cocoa_.get() acceptsFirstResponder]);
  [window makeFirstResponder:rwhv_cocoa_.get()];
  EXPECT_EQ(rwhv_cocoa_.get(), [window firstResponder]);
  EXPECT_FALSE([accelerated_view acceptsFirstResponder]);

  EXPECT_TRUE([other_view acceptsFirstResponder]);
  [window makeFirstResponder:other_view];
  EXPECT_NE(rwhv_cocoa_.get(), [window firstResponder]);

  EXPECT_TRUE([accelerated_view acceptsFirstResponder]);
  [window makeFirstResponder:accelerated_view];
  EXPECT_EQ(rwhv_cocoa_.get(), [window firstResponder]);

  // Clean up.
  DestroyPluginView(accelerated_handle);
}

TEST_F(RenderWidgetHostViewMacTest, AcceptsFirstResponder) {
  // The RWHVCocoa should normally accept first responder status.
  EXPECT_TRUE([rwhv_cocoa_.get() acceptsFirstResponder]);

  // Unless we tell it not to.
  rwhv_mac_->SetTakesFocusOnlyOnMouseDown(true);
  EXPECT_FALSE([rwhv_cocoa_.get() acceptsFirstResponder]);

  // But we can set things back to the way they were originally.
  rwhv_mac_->SetTakesFocusOnlyOnMouseDown(false);
  EXPECT_TRUE([rwhv_cocoa_.get() acceptsFirstResponder]);
}

TEST_F(RenderWidgetHostViewMacTest, TakesFocusOnMouseDown) {
  scoped_nsobject<CocoaTestHelperWindow>
      window([[CocoaTestHelperWindow alloc] init]);
  [[window contentView] addSubview:rwhv_cocoa_.get()];

  // Even if the RWHVCocoa disallows first responder, clicking on it gives it
  // focus.
  [window setPretendIsKeyWindow:YES];
  [window makeFirstResponder:nil];
  ASSERT_NE(rwhv_cocoa_.get(), [window firstResponder]);

  rwhv_mac_->SetTakesFocusOnlyOnMouseDown(true);
  EXPECT_FALSE([rwhv_cocoa_.get() acceptsFirstResponder]);

  std::pair<NSEvent*, NSEvent*> clicks =
      cocoa_test_event_utils::MouseClickInView(rwhv_cocoa_.get(), 1);
  [rwhv_cocoa_.get() mouseDown:clicks.first];
  EXPECT_EQ(rwhv_cocoa_.get(), [window firstResponder]);
}

// Regression test for http://crbug.com/64256
TEST_F(RenderWidgetHostViewMacTest, TakesFocusOnMouseDownWithAcceleratedView) {
  // The accelerated view methods want to be called on the UI thread.
  scoped_ptr<BrowserThreadImpl> ui_thread_(
      new BrowserThreadImpl(BrowserThread::UI,
                            MessageLoop::current()));

  int w = 400, h = 300;
  gfx::PluginWindowHandle accelerated_handle = AddAcceleratedPluginView(w, h);
  EXPECT_FALSE([rwhv_cocoa_.get() isHidden]);
  NSView* accelerated_view = static_cast<NSView*>(
      rwhv_mac_->ViewForPluginWindowHandle(accelerated_handle));
  EXPECT_FALSE([accelerated_view isHidden]);

  // Add the RWHVCocoa to the window and remove first responder status.
  scoped_nsobject<CocoaTestHelperWindow>
      window([[CocoaTestHelperWindow alloc] init]);
  [[window contentView] addSubview:rwhv_cocoa_.get()];
  [window setPretendIsKeyWindow:YES];
  [window makeFirstResponder:nil];
  EXPECT_NE(rwhv_cocoa_.get(), [window firstResponder]);

  // Tell the RWHVMac to not accept first responder status.  The accelerated
  // view should also stop accepting first responder.
  rwhv_mac_->SetTakesFocusOnlyOnMouseDown(true);
  EXPECT_FALSE([accelerated_view acceptsFirstResponder]);

  // A click on the accelerated view should focus the RWHVCocoa.
  std::pair<NSEvent*, NSEvent*> clicks =
      cocoa_test_event_utils::MouseClickInView(accelerated_view, 1);
  [rwhv_cocoa_.get() mouseDown:clicks.first];
  EXPECT_EQ(rwhv_cocoa_.get(), [window firstResponder]);

  // Clean up.
  DestroyPluginView(accelerated_handle);
}

TEST_F(RenderWidgetHostViewMacTest, Fullscreen) {
  rwhv_mac_->InitAsFullscreen(NULL);
  EXPECT_TRUE(rwhv_mac_->pepper_fullscreen_window());
}

// Verify that escape key down in fullscreen mode suppressed the keyup event on
// the parent.
TEST_F(RenderWidgetHostViewMacTest, FullscreenCloseOnEscape) {
  // Use our own RWH since we need to destroy it.
  MockRenderWidgetHostDelegate delegate;
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  // Owned by its |cocoa_view()|.
  RenderWidgetHostImpl* rwh = new RenderWidgetHostImpl(
      &delegate, process_host, MSG_ROUTING_NONE);
  RenderWidgetHostViewMac* view = static_cast<RenderWidgetHostViewMac*>(
      RenderWidgetHostView::CreateViewForWidget(rwh));

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
  // Owned by its |cocoa_view()|.
  RenderWidgetHostImpl* rwh = new RenderWidgetHostImpl(
      &delegate, process_host, MSG_ROUTING_NONE);
  RenderWidgetHostViewMac* view = static_cast<RenderWidgetHostViewMac*>(
      RenderWidgetHostView::CreateViewForWidget(rwh));

  view->InitAsFullscreen(rwhv_mac_);

  WindowedNotificationObserver observer(
      NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,
      Source<RenderWidgetHost>(rwh));

  // Command-ESC will destroy the view, while the window is still in
  // |-performKeyEquivalent:|.  There are other cases where this can
  // happen, Command-ESC is the easiest to trigger.
  [[view->cocoa_view() window] performKeyEquivalent:
      cocoa_test_event_utils::KeyEventWithKeyCode(
          53, 27, NSKeyDown, NSCommandKeyMask)];
  observer.Wait();
}

TEST_F(RenderWidgetHostViewMacTest, GetFirstRectForCharacterRangeCaretCase) {
  const string16 kDummyString = UTF8ToUTF16("hogehoge");
  const size_t kDummyOffset = 0;

  gfx::Rect caret_rect(10, 11, 0, 10);
  ui::Range caret_range(0, 0);
  ViewHostMsg_SelectionBounds_Params params;

  NSRect rect;
  NSRange actual_range;
  rwhv_mac_->SelectionChanged(kDummyString, kDummyOffset, caret_range);
  params.anchor_rect = params.focus_rect = caret_rect;
  params.anchor_dir = params.focus_dir = WebKit::WebTextDirectionLeftToRight;
  rwhv_mac_->SelectionBoundsChanged(params);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        caret_range.ToNSRange(),
        &rect,
        &actual_range));
  EXPECT_EQ(caret_rect, gfx::Rect(NSRectToCGRect(rect)));
  EXPECT_EQ(caret_range, ui::Range(actual_range));

  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        ui::Range(0, 1).ToNSRange(),
        &rect,
        &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        ui::Range(1, 1).ToNSRange(),
        &rect,
        &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        ui::Range(2, 3).ToNSRange(),
        &rect,
        &actual_range));

  // Caret moved.
  caret_rect = gfx::Rect(20, 11, 0, 10);
  caret_range = ui::Range(1, 1);
  params.anchor_rect = params.focus_rect = caret_rect;
  rwhv_mac_->SelectionChanged(kDummyString, kDummyOffset, caret_range);
  rwhv_mac_->SelectionBoundsChanged(params);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        caret_range.ToNSRange(),
        &rect,
        &actual_range));
  EXPECT_EQ(caret_rect, gfx::Rect(NSRectToCGRect(rect)));
  EXPECT_EQ(caret_range, ui::Range(actual_range));

  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        ui::Range(0, 0).ToNSRange(),
        &rect,
        &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        ui::Range(1, 2).ToNSRange(),
        &rect,
        &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        ui::Range(2, 3).ToNSRange(),
        &rect,
        &actual_range));

  // No caret.
  caret_range = ui::Range(1, 2);
  rwhv_mac_->SelectionChanged(kDummyString, kDummyOffset, caret_range);
  params.anchor_rect = caret_rect;
  params.focus_rect = gfx::Rect(30, 11, 0, 10);
  rwhv_mac_->SelectionBoundsChanged(params);
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        ui::Range(0, 0).ToNSRange(),
        &rect,
        &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        ui::Range(0, 1).ToNSRange(),
        &rect,
        &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        ui::Range(1, 1).ToNSRange(),
        &rect,
        &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        ui::Range(1, 2).ToNSRange(),
        &rect,
        &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        ui::Range(2, 2).ToNSRange(),
        &rect,
        &actual_range));
}

TEST_F(RenderWidgetHostViewMacTest, UpdateCompositionSinglelineCase) {
  const gfx::Point kOrigin(10, 11);
  const gfx::Size kBoundsUnit(10, 20);

  NSRect rect;
  // Make sure not crashing by passing NULL pointer instead of |actual_range|.
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      ui::Range(0, 0).ToNSRange(),
      &rect,
      NULL));

  // If there are no update from renderer, always returned caret position.
  NSRange actual_range;
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      ui::Range(0, 0).ToNSRange(),
      &rect,
      &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      ui::Range(0, 1).ToNSRange(),
      &rect,
      &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      ui::Range(1, 0).ToNSRange(),
      &rect,
      &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      ui::Range(1, 1).ToNSRange(),
      &rect,
      &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      ui::Range(1, 2).ToNSRange(),
      &rect,
      &actual_range));

  // If the firstRectForCharacterRange is failed in renderer, empty rect vector
  // is sent. Make sure this does not crash.
  rwhv_mac_->ImeCompositionRangeChanged(ui::Range(10, 12),
                                        std::vector<gfx::Rect>());
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      ui::Range(10, 11).ToNSRange(),
      &rect,
      NULL));

  const int kCompositionLength = 10;
  std::vector<gfx::Rect> composition_bounds;
  const int kCompositionStart = 3;
  const ui::Range kCompositionRange(kCompositionStart,
                                    kCompositionStart + kCompositionLength);
  GenerateCompositionRectArray(kOrigin,
                               kBoundsUnit,
                               kCompositionLength,
                               std::vector<size_t>(),
                               &composition_bounds);
  rwhv_mac_->ImeCompositionRangeChanged(kCompositionRange, composition_bounds);

  // Out of range requests will return caret position.
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      ui::Range(0, 0).ToNSRange(),
      &rect,
      &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      ui::Range(1, 1).ToNSRange(),
      &rect,
      &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      ui::Range(1, 2).ToNSRange(),
      &rect,
      &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      ui::Range(2, 2).ToNSRange(),
      &rect,
      &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      ui::Range(13, 14).ToNSRange(),
      &rect,
      &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      ui::Range(14, 15).ToNSRange(),
      &rect,
      &actual_range));

  for (int i = 0; i <= kCompositionLength; ++i) {
    for (int j = 0; j <= kCompositionLength - i; ++j) {
      const ui::Range range(i, i + j);
      const gfx::Rect expected_rect = GetExpectedRect(kOrigin,
                                                      kBoundsUnit,
                                                      range,
                                                      0);
      const NSRange request_range = ui::Range(
          kCompositionStart + range.start(),
          kCompositionStart + range.end()).ToNSRange();
      EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
            request_range,
            &rect,
            &actual_range));
      EXPECT_EQ(ui::Range(request_range), ui::Range(actual_range));
      EXPECT_EQ(expected_rect, gfx::Rect(NSRectToCGRect(rect)));

      // Make sure not crashing by passing NULL pointer instead of
      // |actual_range|.
      EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
            request_range,
            &rect,
            NULL));
    }
  }
}

TEST_F(RenderWidgetHostViewMacTest, UpdateCompositionMultilineCase) {
  const gfx::Point kOrigin(10, 11);
  const gfx::Size kBoundsUnit(10, 20);
  NSRect rect;

  const int kCompositionLength = 30;
  std::vector<gfx::Rect> composition_bounds;
  const ui::Range kCompositionRange(0, kCompositionLength);
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
  ui::Range range;
  range = ui::Range(5, 8);
  NSRange actual_range;
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(range, ui::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, range, 0),
      gfx::Rect(NSRectToCGRect(rect)));
  range = ui::Range(15, 18);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(range, ui::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, ui::Range(5, 8), 1),
      gfx::Rect(NSRectToCGRect(rect)));
  range = ui::Range(25, 28);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(range, ui::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, ui::Range(5, 8), 2),
      gfx::Rect(NSRectToCGRect(rect)));

  // Range contains line breaking point.
  range = ui::Range(8, 12);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(ui::Range(8, 10), ui::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, ui::Range(8, 10), 0),
      gfx::Rect(NSRectToCGRect(rect)));
  range = ui::Range(18, 22);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(ui::Range(18, 20), ui::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, ui::Range(8, 10), 1),
      gfx::Rect(NSRectToCGRect(rect)));

  // Start point is line breaking point.
  range = ui::Range(10, 12);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(ui::Range(10, 12), ui::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, ui::Range(0, 2), 1),
      gfx::Rect(NSRectToCGRect(rect)));
  range = ui::Range(20, 22);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(ui::Range(20, 22), ui::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, ui::Range(0, 2), 2),
      gfx::Rect(NSRectToCGRect(rect)));

  // End point is line breaking point.
  range = ui::Range(5, 10);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(ui::Range(5, 10), ui::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, ui::Range(5, 10), 0),
      gfx::Rect(NSRectToCGRect(rect)));
  range = ui::Range(15, 20);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(ui::Range(15, 20), ui::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, ui::Range(5, 10), 1),
      gfx::Rect(NSRectToCGRect(rect)));

  // Start and end point are same line breaking point.
  range = ui::Range(10, 10);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(ui::Range(10, 10), ui::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, ui::Range(0, 0), 1),
      gfx::Rect(NSRectToCGRect(rect)));
  range = ui::Range(20, 20);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(ui::Range(20, 20), ui::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, ui::Range(0, 0), 2),
      gfx::Rect(NSRectToCGRect(rect)));

  // Start and end point are different line breaking point.
  range = ui::Range(10, 20);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(ui::Range(10, 20), ui::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, ui::Range(0, 10), 1),
      gfx::Rect(NSRectToCGRect(rect)));
}

// Verify that |SetActive()| calls |RenderWidgetHostImpl::Blur()| and
// |RenderWidgetHostImp::Focus()|.
TEST_F(RenderWidgetHostViewMacTest, BlurAndFocusOnSetActive) {
  MockRenderWidgetHostDelegate delegate;
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);

  // Owned by its |cocoa_view()|.
  MockRenderWidgetHostImpl* rwh = new MockRenderWidgetHostImpl(
      &delegate, process_host, MSG_ROUTING_NONE);
  RenderWidgetHostViewMac* view = static_cast<RenderWidgetHostViewMac*>(
      RenderWidgetHostView::CreateViewForWidget(rwh));

  scoped_nsobject<CocoaTestHelperWindow>
      window([[CocoaTestHelperWindow alloc] init]);
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
  rwh->Shutdown();
}

TEST_F(RenderWidgetHostViewMacTest, ScrollWheelEndEventDelivery) {
  // This tests Lion+ functionality, so don't run the test pre-Lion.
  if (!base::mac::IsOSLionOrLater())
    return;

  // Initialize the view associated with a MockRenderWidgetHostImpl, rather than
  // the MockRenderProcessHost that is set up by the test harness which mocks
  // out |OnMessageReceived()|.
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  MockRenderWidgetHostDelegate delegate;
  MockRenderWidgetHostImpl* host = new MockRenderWidgetHostImpl(
      &delegate, process_host, MSG_ROUTING_NONE);
  RenderWidgetHostViewMac* view = static_cast<RenderWidgetHostViewMac*>(
      RenderWidgetHostView::CreateViewForWidget(host));

  // Send an initial wheel event with NSEventPhaseBegan to the view.
  NSEvent* event1 = MockScrollWheelEventWithPhase(@selector(phaseBegan));
  [view->cocoa_view() scrollWheel:event1];
  ASSERT_EQ(1U, process_host->sink().message_count());

  // Send an ACK for the first wheel event, so that the queue will be flushed.
  scoped_ptr<IPC::Message> response(
      new ViewHostMsg_HandleInputEvent_ACK(0, WebKit::WebInputEvent::MouseWheel,
                                           INPUT_EVENT_ACK_STATE_CONSUMED));
  host->OnMessageReceived(*response);

  // Post the NSEventPhaseEnded wheel event to NSApp and check whether the
  // render view receives it.
  NSEvent* event2 = MockScrollWheelEventWithPhase(@selector(phaseEnded));
  [NSApp postEvent:event2 atStart:NO];
  MessageLoop::current()->RunUntilIdle();
  ASSERT_EQ(2U, process_host->sink().message_count());

  // Clean up.
  host->Shutdown();
}

}  // namespace content
