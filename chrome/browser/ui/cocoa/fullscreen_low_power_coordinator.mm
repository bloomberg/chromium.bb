// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/fullscreen_low_power_coordinator.h"

namespace {

// The minimum number of frames with valid low power contents that we need to
// receive in a row before showing the low power window.
const uint64_t kMinValidFrames = 15;

}  // namespace

@interface FullscreenLowPowerWindow : NSWindow {
  base::scoped_nsobject<NSWindow> eventTargetWindow_;
}
- (id)initWithEventTargetWindow:(NSWindow*)eventTargetWindow
                      withLayer:(CALayer*)layer;
@end

@implementation FullscreenLowPowerWindow
- (id)initWithEventTargetWindow:(NSWindow*)eventTargetWindow
                      withLayer:(CALayer*)layer {
  if (self = [super
          initWithContentRect:NSMakeRect(0, 0, 256, 256)
                    styleMask:NSTitledWindowMask | NSResizableWindowMask |
                              NSFullSizeContentViewWindowMask
                      backing:NSBackingStoreBuffered
                        defer:NO]) {
    eventTargetWindow_.reset(eventTargetWindow, base::scoped_policy::RETAIN);
    [self setCollectionBehavior:NSWindowCollectionBehaviorIgnoresCycle];
    [self setExcludedFromWindowsMenu:YES];
    [self setReleasedWhenClosed:NO];
    [self setIgnoresMouseEvents:YES];

    base::scoped_nsobject<CALayer> content_layer([[CALayer alloc] init]);
    [content_layer setBackgroundColor:CGColorGetConstantColor(kCGColorBlack)];

    NSView* content_view = [self contentView];
    [content_view setLayer:content_layer];
    [content_view setWantsLayer:YES];

    [content_layer addSublayer:layer];
  }
  return self;
}

- (void)sendEvent:(NSEvent*)event {
  [eventTargetWindow_ sendEvent:event];
}
@end

FullscreenLowPowerCoordinatorCocoa::FullscreenLowPowerCoordinatorCocoa(
    NSWindow* content_window,
    ui::AcceleratedWidgetMac* widget)
    : content_window_(content_window, base::scoped_policy::RETAIN),
      widget_(widget) {
  if (widget_) {
    widget_->SetFullscreenLowPowerCoordinator(this);
    CALayer* fullscreen_low_power_layer = widget_->GetFullscreenLowPowerLayer();
    if (fullscreen_low_power_layer) {
      low_power_window_.reset([[FullscreenLowPowerWindow alloc]
          initWithEventTargetWindow:content_window_
                          withLayer:fullscreen_low_power_layer]);
    }
  }

  SetHasActiveSheet([content_window_ attachedSheet]);
  ChildWindowsChanged();
}

FullscreenLowPowerCoordinatorCocoa::~FullscreenLowPowerCoordinatorCocoa() {
  if (widget_)
    widget_->ResetFullscreenLowPowerCoordinator();
  EnterOrExitLowPowerModeIfNeeded();
  [low_power_window_ close];
}

NSWindow* FullscreenLowPowerCoordinatorCocoa::GetFullscreenLowPowerWindow() {
  return low_power_window_.get();
}

void FullscreenLowPowerCoordinatorCocoa::SetInFullscreenTransition(
    bool in_fullscreen_transition) {
  allowed_by_fullscreen_transition_ = !in_fullscreen_transition;
  EnterOrExitLowPowerModeIfNeeded();
}

void FullscreenLowPowerCoordinatorCocoa::SetLayoutParameters(
    const NSRect& toolbar_frame,
    const NSRect& infobar_frame,
    const NSRect& content_frame,
    const NSRect& download_shelf_frame) {
  NSRect screen_frame = [[content_window_ screen] frame];
  allowed_by_nsview_layout_ = true;

  // The toolbar and infobar must be above the top of the screen.
  allowed_by_nsview_layout_ &=
      toolbar_frame.origin.y >= screen_frame.size.height;
  allowed_by_nsview_layout_ &=
      infobar_frame.origin.y >= screen_frame.size.height;

  // The content rect must equal the screen's rect.
  allowed_by_nsview_layout_ &= NSEqualRects(content_frame, screen_frame);

  // The download shelf must not extend on to the screen.
  allowed_by_nsview_layout_ &=
      download_shelf_frame.origin.y + download_shelf_frame.size.height <= 0;

  EnterOrExitLowPowerModeIfNeeded();
}

void FullscreenLowPowerCoordinatorCocoa::SetHasActiveSheet(bool has_sheet) {
  allowed_by_active_sheet_ = !has_sheet;
  EnterOrExitLowPowerModeIfNeeded();
}

void FullscreenLowPowerCoordinatorCocoa::ChildWindowsChanged() {
  allowed_by_child_windows_ = true;
  for (NSWindow* child_window in [content_window_ childWindows]) {
    // The toolbar correctly appears on top of the fullscreen low power window.
    if ([child_window
            isKindOfClass:NSClassFromString(@"NSToolbarFullScreenWindow")]) {
      continue;
    }
    // There is a persistent 1x1 StatusBubbleWindow child window at 0,0.
    if ([child_window isKindOfClass:NSClassFromString(@"StatusBubbleWindow")] &&
        NSEqualRects([child_window frame], NSMakeRect(0, 0, 1, 1))) {
      continue;
    }

    // Don't make any assumptions about other child windows.
    allowed_by_child_windows_ = false;
    break;
  }

  EnterOrExitLowPowerModeIfNeeded();
}

void FullscreenLowPowerCoordinatorCocoa::SetLowPowerLayerValid(bool valid) {
  if (valid)
    low_power_layer_valid_frame_count_ += 1;
  else
    low_power_layer_valid_frame_count_ = 0;
  EnterOrExitLowPowerModeIfNeeded();
}

void FullscreenLowPowerCoordinatorCocoa::WillLoseAcceleratedWidget() {
  DCHECK(widget_);
  widget_->ResetFullscreenLowPowerCoordinator();
  widget_ = nullptr;
  EnterOrExitLowPowerModeIfNeeded();
}

void FullscreenLowPowerCoordinatorCocoa::EnterOrExitLowPowerModeIfNeeded() {
  bool new_in_low_power_mode =
      widget_ && low_power_window_ &&
      low_power_layer_valid_frame_count_ > kMinValidFrames &&
      allowed_by_fullscreen_transition_ && allowed_by_nsview_layout_ &&
      allowed_by_child_windows_ && allowed_by_active_sheet_;

  if (new_in_low_power_mode) {
    // Update whether or not we are in low power mode based on whether or not
    // the low power window is in front (we do not get notifications of window
    // order change).
    in_low_power_mode_ &=
        [[NSApp orderedWindows] firstObject] == low_power_window_.get();

    if (!in_low_power_mode_) {
      [low_power_window_ setFrame:[content_window_ frame] display:YES];
      [low_power_window_
          setStyleMask:[low_power_window_ styleMask] | NSFullScreenWindowMask];
      [low_power_window_ orderWindow:NSWindowAbove
                          relativeTo:[content_window_ windowNumber]];
      in_low_power_mode_ = true;
    }
  } else {
    if (in_low_power_mode_) {
      [low_power_window_ orderWindow:NSWindowBelow
                          relativeTo:[content_window_ windowNumber]];
      in_low_power_mode_ = false;
    }
  }
}
