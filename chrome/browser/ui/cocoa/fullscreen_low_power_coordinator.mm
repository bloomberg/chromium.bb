// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/fullscreen_low_power_coordinator.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"

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

void FullscreenLowPowerCoordinatorCocoa::SetLowPowerLayerValid(bool valid) {
  low_power_layer_valid_ = valid;
  EnterOrExitLowPowerModeIfNeeded();
}

void FullscreenLowPowerCoordinatorCocoa::WillLoseAcceleratedWidget() {
  DCHECK(widget_);
  widget_->ResetFullscreenLowPowerCoordinator();
  widget_ = nullptr;
  EnterOrExitLowPowerModeIfNeeded();
}

void FullscreenLowPowerCoordinatorCocoa::EnterOrExitLowPowerModeIfNeeded() {
  static bool enabled_at_command_line =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableFullscreenLowPowerMode);

  bool new_in_low_power_mode =
      widget_ && low_power_window_ && allowed_by_nsview_layout_ &&
      low_power_layer_valid_ && enabled_at_command_line;

  if (new_in_low_power_mode) {
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
