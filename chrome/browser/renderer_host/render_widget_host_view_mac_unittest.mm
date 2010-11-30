// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_widget_host_view_mac.h"

#include "chrome/browser/browser_thread.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/browser/cocoa/test_event_utils.h"
#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"

class RenderWidgetHostViewMacTest : public RenderViewHostTestHarness {
 public:
  RenderWidgetHostViewMacTest() {}

  virtual void SetUp() {
    RenderViewHostTestHarness::SetUp();

    // Owned by its |native_view()|.
    rwhv_mac_ = new RenderWidgetHostViewMac(rvh());

    // Will be released when the superclass's RenderWidgetHost is destroyed.
    rwhv_cocoa_ = [rwhv_mac_->native_view() retain];
  }
 protected:
  // Adds an accelerated plugin view to |rwhv_cocoa_|.  Returns a handle to the
  // newly-added view.  Callers must ensure that a UI thread is present and
  // running before calling this function.
  gfx::PluginWindowHandle AddAcceleratedPluginView(int w, int h) {
    // Create an accelerated view the size of the rhwvmac.
    [rwhv_cocoa_ setFrame:NSMakeRect(0, 0, w, h)];
    gfx::PluginWindowHandle accelerated_handle =
        rwhv_mac_->AllocateFakePluginWindowHandle(/*opaque=*/false,
                                                  /*root=*/false);
    rwhv_mac_->AcceleratedSurfaceSetIOSurface(accelerated_handle, w, h, 0);

    // The accelerated view isn't shown until it has a valid rect and has been
    // painted to.
    rwhv_mac_->AcceleratedSurfaceBuffersSwapped(accelerated_handle, 0, 0, 0, 0);
    webkit_glue::WebPluginGeometry geom;
    gfx::Rect rect(0, 0, w, h);
    geom.window = accelerated_handle;
    geom.window_rect = rect;
    geom.clip_rect = rect;
    geom.visible = true;
    geom.rects_valid = true;
    rwhv_mac_->MovePluginWindows(
        std::vector<webkit_glue::WebPluginGeometry>(1, geom));

    return accelerated_handle;
  }

  RenderWidgetHostViewMac* rwhv_mac_;
  RenderWidgetHostViewCocoa* rwhv_cocoa_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewMacTest);
};

TEST_F(RenderWidgetHostViewMacTest, Basic) {
}

// Regression test for http://crbug.com/60318
TEST_F(RenderWidgetHostViewMacTest, FocusAcceleratedView) {
  // The accelerated view methods want to be called on the UI thread.
  scoped_ptr<BrowserThread> ui_thread_(
      new BrowserThread(BrowserThread::UI, MessageLoop::current()));

  int w = 400, h = 300;
  gfx::PluginWindowHandle accelerated_handle = AddAcceleratedPluginView(w, h);
  EXPECT_FALSE([rwhv_cocoa_ isHidden]);
  NSView* accelerated_view = static_cast<NSView*>(
      rwhv_mac_->ViewForPluginWindowHandle(accelerated_handle));
  EXPECT_FALSE([accelerated_view isHidden]);

  // Take away first responder from the rwhvmac, then simulate the effect of a
  // click on the accelerated view. The rwhvmac should be first responder
  // again.
  scoped_nsobject<NSWindow> window([[CocoaTestHelperWindow alloc] init]);
  scoped_nsobject<NSView> other_view(
      [[NSTextField alloc] initWithFrame:NSMakeRect(0, h, w, 40)]);
  [[window contentView] addSubview:rwhv_cocoa_];
  [[window contentView] addSubview:other_view.get()];

  EXPECT_TRUE([rwhv_cocoa_ acceptsFirstResponder]);
  [window makeFirstResponder:rwhv_cocoa_];
  EXPECT_EQ(rwhv_cocoa_, [window firstResponder]);
  EXPECT_FALSE([accelerated_view acceptsFirstResponder]);

  EXPECT_TRUE([other_view acceptsFirstResponder]);
  [window makeFirstResponder:other_view];
  EXPECT_NE(rwhv_cocoa_, [window firstResponder]);

  EXPECT_TRUE([accelerated_view acceptsFirstResponder]);
  [window makeFirstResponder:accelerated_view];
  EXPECT_EQ(rwhv_cocoa_, [window firstResponder]);

  // Clean up.
  rwhv_mac_->DestroyFakePluginWindowHandle(accelerated_handle);
}

TEST_F(RenderWidgetHostViewMacTest, AcceptsFirstResponder) {
  // The RWHVCocoa should normally accept first responder status.
  EXPECT_TRUE([rwhv_cocoa_ acceptsFirstResponder]);

  // Unless we tell it not to.
  rwhv_mac_->SetTakesFocusOnlyOnMouseDown(true);
  EXPECT_FALSE([rwhv_cocoa_ acceptsFirstResponder]);

  // But we can set things back to the way they were originally.
  rwhv_mac_->SetTakesFocusOnlyOnMouseDown(false);
  EXPECT_TRUE([rwhv_cocoa_ acceptsFirstResponder]);
}

TEST_F(RenderWidgetHostViewMacTest, TakesFocusOnMouseDown) {
  scoped_nsobject<NSWindow> window([[CocoaTestHelperWindow alloc] init]);
  [[window contentView] addSubview:rwhv_cocoa_];

  // Even if the RWHVCocoa disallows first responder, clicking on it gives it
  // focus.
  [window makeFirstResponder:nil];
  ASSERT_NE(rwhv_cocoa_, [window firstResponder]);

  rwhv_mac_->SetTakesFocusOnlyOnMouseDown(true);
  EXPECT_FALSE([rwhv_cocoa_ acceptsFirstResponder]);

  std::pair<NSEvent*, NSEvent*> clicks =
      test_event_utils::MouseClickInView(rwhv_cocoa_, 1);
  [rwhv_cocoa_ mouseDown:clicks.first];
  EXPECT_EQ(rwhv_cocoa_, [window firstResponder]);
}

// Regression test for http://crbug.com/64256
TEST_F(RenderWidgetHostViewMacTest, TakesFocusOnMouseDownWithAcceleratedView) {
  // The accelerated view methods want to be called on the UI thread.
  scoped_ptr<BrowserThread> ui_thread_(
      new BrowserThread(BrowserThread::UI, MessageLoop::current()));

  int w = 400, h = 300;
  gfx::PluginWindowHandle accelerated_handle = AddAcceleratedPluginView(w, h);
  EXPECT_FALSE([rwhv_cocoa_ isHidden]);
  NSView* accelerated_view = static_cast<NSView*>(
      rwhv_mac_->ViewForPluginWindowHandle(accelerated_handle));
  EXPECT_FALSE([accelerated_view isHidden]);

  // Add the RWHVCocoa to the window and remove first responder status.
  scoped_nsobject<NSWindow> window([[CocoaTestHelperWindow alloc] init]);
  [[window contentView] addSubview:rwhv_cocoa_];
  [window makeFirstResponder:nil];
  EXPECT_NE(rwhv_cocoa_, [window firstResponder]);

  // Tell the RWHVMac to not accept first responder status.  The accelerated
  // view should also stop accepting first responder.
  rwhv_mac_->SetTakesFocusOnlyOnMouseDown(true);
  EXPECT_FALSE([accelerated_view acceptsFirstResponder]);

  // A click on the accelerated view should focus the RWHVCocoa.
  std::pair<NSEvent*, NSEvent*> clicks =
      test_event_utils::MouseClickInView(accelerated_view, 1);
  [rwhv_cocoa_ mouseDown:clicks.first];
  EXPECT_EQ(rwhv_cocoa_, [window firstResponder]);

  // Clean up.
  rwhv_mac_->DestroyFakePluginWindowHandle(accelerated_handle);
}
