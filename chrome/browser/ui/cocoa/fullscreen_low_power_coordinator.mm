// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/fullscreen_low_power_coordinator.h"

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

void FullscreenLowPowerCoordinatorCocoa::AddLowPowerModeSuppression() {
  suppression_count_ += 1;
  EnterOrExitLowPowerModeIfNeeded();
}

void FullscreenLowPowerCoordinatorCocoa::RemoveLowPowerModeSuppression() {
  DCHECK(suppression_count_ > 0);
  suppression_count_ -= 1;
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
  bool new_in_low_power_mode = widget_ && low_power_window_ &&
                               !suppression_count_ && low_power_layer_valid_;
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
