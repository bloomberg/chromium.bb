// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/fullscreen_low_power_coordinator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"

namespace {

const uint64_t kNotEnoughFrames = 5;
const uint64_t kEnoughFrames = 20;

class FullscreenLowPowerTest : public testing::Test,
                               public ui::AcceleratedWidgetMacNSView {
 public:
  // ui::AcceleratedWidgetMacNSView implementation.
  NSView* AcceleratedWidgetGetNSView() const override { return view_.get(); }
  void AcceleratedWidgetGetVSyncParameters(
      base::TimeTicks* timebase,
      base::TimeDelta* interval) const override {}
  void AcceleratedWidgetSwapCompleted() override {}

  // testing::Test implementation.
  void SetUp() override {
    base::scoped_nsobject<CALayer> background_layer([[CALayer alloc] init]);
    view_.reset([[NSView alloc] initWithFrame:NSMakeRect(0, 0, 1, 1)]);
    [view_ setLayer:background_layer.get()];
    [view_ setWantsLayer:YES];

    content_window_.reset([[NSWindow alloc]
        initWithContentRect:NSMakeRect(0, 0, 256, 256)
                  styleMask:NSTitledWindowMask | NSResizableWindowMask
                    backing:NSBackingStoreBuffered
                      defer:NO]);
    [content_window_ setReleasedWhenClosed:NO];
    [content_window_ orderFront:nil];

    widget_.reset(new ui::AcceleratedWidgetMac);
    RecreateLayers();

    widget_->SetNSView(this);
    // Initialize. Note that we need to send at least one frame before creating
    // the coordinator.
    SendFrames(1, true);
    coordinator_.reset(new FullscreenLowPowerCoordinatorCocoa(
        content_window_.get(), widget_.get()));
  }
  void TearDown() override {
    widget_->ResetNSView();
    [content_window_ close];
  }

  void RecreateLayers() {
    content_layer_.reset([[CALayer alloc] init]);
    low_power_layer_.reset([[CALayer alloc] init]);
  }
  void SendFrames(int count, bool low_power_valid) {
    for (int i = 0; i < count; ++i)
      widget_->GotCALayerFrame(content_layer_, low_power_valid,
                               low_power_layer_, gfx::Size(1, 1), 1);
  }
  bool DoesTransitionToLowPowerMode() {
    if (IsInLowPowerMode())
      return false;
    for (int i = 0; i < 20; ++i)
      coordinator_->TickEnterOrExitForTesting();
    return IsInLowPowerMode();
  }
  bool IsInLowPowerMode() {
    // Return true if the fullscreen low power window is in front of the content
    // window.
    for (NSWindow* window in [NSApp orderedWindows]) {
      if (window == content_window_.get())
        return false;
      if (window == coordinator_->GetFullscreenLowPowerWindow())
        return true;
    }
    return false;
  }

  std::unique_ptr<ui::AcceleratedWidgetMac> widget_;
  base::scoped_nsobject<NSView> view_;
  base::scoped_nsobject<NSWindow> content_window_;

  base::scoped_nsobject<CALayer> content_layer_;
  base::scoped_nsobject<CALayer> low_power_layer_;

  std::unique_ptr<FullscreenLowPowerCoordinatorCocoa> coordinator_;
};

TEST_F(FullscreenLowPowerTest, DestroyOrder) {
  widget_->ResetNSView();
  coordinator_.reset();
  widget_->SetNSView(this);

  // Verify that the coordinator can be destroyed while attached to the widget.
  EXPECT_FALSE(widget_->MightBeInFullscreenLowPowerMode());
  coordinator_.reset(new FullscreenLowPowerCoordinatorCocoa(
      content_window_.get(), widget_.get()));
  EXPECT_TRUE(widget_->MightBeInFullscreenLowPowerMode());
  coordinator_.reset();
  EXPECT_FALSE(widget_->MightBeInFullscreenLowPowerMode());

  // Verify that the widget can be destroyed while the coordinator exists.
  coordinator_.reset(new FullscreenLowPowerCoordinatorCocoa(
      content_window_.get(), widget_.get()));
  EXPECT_TRUE(widget_->MightBeInFullscreenLowPowerMode());
  widget_->ResetNSView();
  coordinator_.reset();
}

TEST_F(FullscreenLowPowerTest, TransitionAndHysteresis) {
  // Ensure that we can't enter low power mode until we finish the fullscreen
  // transition.
  EXPECT_FALSE(IsInLowPowerMode());
  SendFrames(kEnoughFrames, true);
  EXPECT_FALSE(IsInLowPowerMode());
  coordinator_->SetInFullscreenTransition(false);
  EXPECT_TRUE(DoesTransitionToLowPowerMode());
  coordinator_->SetInFullscreenTransition(true);
  EXPECT_FALSE(IsInLowPowerMode());

  // Verify hysteresis.
  coordinator_->SetInFullscreenTransition(false);
  EXPECT_TRUE(DoesTransitionToLowPowerMode());
  SendFrames(1, false);
  EXPECT_FALSE(IsInLowPowerMode());
  SendFrames(kNotEnoughFrames, true);
  EXPECT_FALSE(DoesTransitionToLowPowerMode());
  SendFrames(kEnoughFrames, true);
  EXPECT_TRUE(IsInLowPowerMode());
}

TEST_F(FullscreenLowPowerTest, Layout) {
  SendFrames(kEnoughFrames, true);
  coordinator_->SetInFullscreenTransition(false);
  EXPECT_TRUE(DoesTransitionToLowPowerMode());
  NSRect screen_frame = [[content_window_ screen] frame];

  // Test a valid layout first (and again after each invalid layout).
  auto set_valid_layout = [this, screen_frame]{
    coordinator_->SetLayoutParameters(
        NSMakeRect(0, screen_frame.size.height, 10, 10),
        NSMakeRect(0, screen_frame.size.height, 10, 10),
        screen_frame,
        NSMakeRect(0, 0, 20, 0));
  };
  set_valid_layout();
  EXPECT_TRUE(IsInLowPowerMode());

  // Invalid layout -- toolbar visible.
  coordinator_->SetLayoutParameters(
      NSMakeRect(0, screen_frame.size.height - 5, 10, 10),
      NSMakeRect(0, screen_frame.size.height, 10, 10),
      screen_frame,
      NSMakeRect(0, 0, 20, 0));
  EXPECT_FALSE(IsInLowPowerMode());
  set_valid_layout();
  EXPECT_TRUE(DoesTransitionToLowPowerMode());

  // Invalid layout -- infobar visible.
  coordinator_->SetLayoutParameters(
      NSMakeRect(0, screen_frame.size.height, 10, 10),
      NSMakeRect(0, screen_frame.size.height - 5, 10, 10),
      screen_frame,
      NSMakeRect(0, 0, 20, 0));
  EXPECT_FALSE(IsInLowPowerMode());
  set_valid_layout();
  EXPECT_TRUE(DoesTransitionToLowPowerMode());

  // Invalid layout -- not fullscreen.
  coordinator_->SetLayoutParameters(
      NSMakeRect(0, screen_frame.size.height, 10, 10),
      NSMakeRect(0, screen_frame.size.height, 10, 10),
      NSMakeRect(screen_frame.origin.x, screen_frame.origin.y,
                 screen_frame.size.width, screen_frame.size.height / 2),
      NSMakeRect(0, 0, 20, 0));
  EXPECT_FALSE(IsInLowPowerMode());
  set_valid_layout();
  EXPECT_TRUE(DoesTransitionToLowPowerMode());

  // Invalid layout -- download shelf visible.
  coordinator_->SetLayoutParameters(
      NSMakeRect(0, screen_frame.size.height, 10, 10),
      NSMakeRect(0, screen_frame.size.height - 5, 10, 10),
      screen_frame,
      NSMakeRect(0, 0, 20, 20));
  EXPECT_FALSE(IsInLowPowerMode());
  set_valid_layout();
  EXPECT_TRUE(DoesTransitionToLowPowerMode());
}

TEST_F(FullscreenLowPowerTest, OrderFrontAndChildWindows) {
  SendFrames(kEnoughFrames, true);
  coordinator_->SetInFullscreenTransition(false);
  EXPECT_TRUE(DoesTransitionToLowPowerMode());

  // Create a child window.
  base::scoped_nsobject<NSWindow> child_window([[NSWindow alloc]
      initWithContentRect:NSMakeRect(0, 0, 256, 256)
                styleMask:NSTitledWindowMask | NSResizableWindowMask
                  backing:NSBackingStoreBuffered
                    defer:NO]);
  [child_window setReleasedWhenClosed:NO];
  [child_window orderFront:nil];

  // The orderFront puts the window in front of the low power window, but
  // the low power window is still in front of the content window, so we
  // still think that we're in low power mode.
  EXPECT_TRUE(IsInLowPowerMode());
  SendFrames(1, true);
  EXPECT_TRUE(IsInLowPowerMode());

  // Now make this be a child window, and ensure that we're no longer in low
  // power mode.
  [content_window_ addChildWindow:child_window ordered:NSWindowAbove];
  coordinator_->ChildWindowsChanged();
  EXPECT_FALSE(IsInLowPowerMode());

  // And remove it, and ensure that we're back in low power mode.
  [content_window_ removeChildWindow:child_window];
  coordinator_->ChildWindowsChanged();
  EXPECT_TRUE(DoesTransitionToLowPowerMode());

  // Now manually send the content window to the front, and make sure that
  // the next frame restores the low power window.
  [content_window_ orderFront:nil];
  EXPECT_FALSE(IsInLowPowerMode());
  SendFrames(1, true);
  EXPECT_TRUE(IsInLowPowerMode());
}

}  // namespace
