// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/fullscreen_low_power_coordinator.h"

#include "ui/base/cocoa/animation_utils.h"
#include "ui/gfx/mac/scoped_cocoa_disable_screen_updates.h"

namespace {

// The number of frames we need to be in the WarmUp state before moving to the
// Enabled state.
const uint64_t kWarmUpFramesBeforeEnteringLowPowerMode = 30;

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
  // Resizing an NSWindow can be slow, so try to guess the right size at
  // creation.
  NSRect screenFrame = [[eventTargetWindow screen] frame];
  NSRect initialRect = NSMakeRect(
      0, 0, NSWidth(screenFrame), NSHeight(screenFrame));
  if (self = [super
          initWithContentRect:initialRect
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
      [low_power_window_ setAnimationBehavior:NSWindowAnimationBehaviorNone];
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

  // Note that we should close the window here so that it does not appear in
  // the background during the transition back to windowed mode. Closing a
  // window that is in detached mode results in a janky transition and system
  // crashes.
  // TODO(ccameron): Allow closing the window here if we have not been in
  // detached mode for several frames.
  // https://crbug.com/644133
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
  State old_state = state_;

  low_power_layer_valid_ = valid;
  EnterOrExitLowPowerModeIfNeeded();

  if (state_ == old_state)
    frames_in_state_ += 1;
  else
    frames_in_state_ = 0;
}

void FullscreenLowPowerCoordinatorCocoa::WillLoseAcceleratedWidget() {
  DCHECK(widget_);
  widget_->ResetFullscreenLowPowerCoordinator();
  widget_ = nullptr;
  EnterOrExitLowPowerModeIfNeeded();
}

void FullscreenLowPowerCoordinatorCocoa::TickEnterOrExitForTesting() {
  frames_in_state_ += 1;
  EnterOrExitLowPowerModeIfNeeded();
}

void FullscreenLowPowerCoordinatorCocoa::EnterOrExitLowPowerModeIfNeeded() {
  bool can_use_low_power_window =
      widget_ && low_power_window_ && low_power_layer_valid_ &&
      allowed_by_fullscreen_transition_ && allowed_by_nsview_layout_ &&
      allowed_by_child_windows_ && allowed_by_active_sheet_;

  switch (state_) {
    case Disabled:
      if (can_use_low_power_window) {
        // Ensure that the window's frame and style are set, and order it behind
        // the main window, so that it's ready to be moved in front.
        state_ = WarmingUp;

        // Changing the size of the Window will also change the size of the
        // views/layers. Disable implicit animations.
        ScopedCAActionDisabler disabler;
        [low_power_window_ setStyleMask:[low_power_window_ styleMask] |
                                        NSFullScreenWindowMask];
        [low_power_window_ setFrame:[content_window_ frame]
                            display:YES
                            animate:NO];
        [low_power_window_ orderWindow:NSWindowBelow
                            relativeTo:[content_window_ windowNumber]];
      }
      break;
    case WarmingUp:
      if (can_use_low_power_window) {
        // After a few frames, move in front of the main window.
        if (frames_in_state_ > kWarmUpFramesBeforeEnteringLowPowerMode) {
          state_ = Enabled;
          [low_power_window_ orderWindow:NSWindowAbove
                              relativeTo:[content_window_ windowNumber]];
        }
      } else {
        state_ = Disabled;
      }
      break;
    case Enabled:
      if (can_use_low_power_window) {
        // We do not get notifications of window order change. Check here that
        // the low power window is in front (and move it there if it isn't).
        if ([[NSApp orderedWindows] firstObject] != low_power_window_.get()) {
          [low_power_window_ orderWindow:NSWindowAbove
                              relativeTo:[content_window_ windowNumber]];
        }
      } else {
        // Move behind the main window.
        state_ = Disabled;
        [low_power_window_ orderWindow:NSWindowBelow
                            relativeTo:[content_window_ windowNumber]];
      }
      break;
  }
}
