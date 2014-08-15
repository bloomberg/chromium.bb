// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/fullscreen_mode_controller.h"

#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_controller_private.h"
#import "chrome/browser/ui/cocoa/fast_resize_view.h"

@interface FullscreenModeController (Private)
- (void)startAnimationToState:(FullscreenToolbarState)state;
- (void)setMenuBarRevealProgress:(CGFloat)progress;
- (void)setRevealAnimationProgress:(NSAnimationProgress)progress;
@end

namespace {

OSStatus MenuBarRevealHandler(EventHandlerCallRef handler,
                              EventRef event,
                              void* context) {
  FullscreenModeController* self =
      static_cast<FullscreenModeController*>(context);
  CGFloat revealFraction = 0;
  GetEventParameter(event, FOUR_CHAR_CODE('rvlf'), typeCGFloat, NULL,
      sizeof(CGFloat), NULL, &revealFraction);
  [self setMenuBarRevealProgress:revealFraction];
  return CallNextEventHandler(handler, event);
}

// The duration of the animation to bring down the tabstrip.
const NSTimeInterval kAnimationDuration = 0.35;

// The height of the tracking area in which mouse entered/exit events will
// reveal the tabstrip.
const NSUInteger kTrackingAreaHeight = 100;

// There is a tiny gap between the window's max Y and the top of the screen,
// which will produce a mouse exit event when the menu bar should be revealed.
// This factor is the size of that gap plus padding.
const CGFloat kTrackingAreaMaxYEpsilon = 15;

}  // namespace

// Animation ///////////////////////////////////////////////////////////////////

@interface FullscreenModeDropDownAnimation : NSAnimation {
  FullscreenModeController* controller_;
}
@end

@implementation FullscreenModeDropDownAnimation

- (id)initWithFullscreenModeController:(FullscreenModeController*)controller {
  if ((self = [super initWithDuration:kAnimationDuration
                       animationCurve:NSAnimationEaseInOut])) {
    controller_ = controller;
    [self setAnimationBlockingMode:NSAnimationNonblocking];
    [self setDelegate:controller_];
  }
  return self;
}

- (void)setCurrentProgress:(NSAnimationProgress)progress {
  [controller_ setRevealAnimationProgress:progress];
}

@end

// Implementation //////////////////////////////////////////////////////////////

@implementation FullscreenModeController

- (id)initWithBrowserWindowController:(BrowserWindowController*)bwc {
  if ((self = [super init])) {
    controller_ = bwc;

    currentState_ = kFullscreenToolbarOnly;
    destinationState_ = kFullscreenToolbarOnly;

    // Create the tracking area at the top of the window.
    NSRect windowFrame = [[bwc window] frame];
    NSRect trackingRect = NSMakeRect(
        0, NSHeight(windowFrame) - kTrackingAreaHeight,
        NSWidth(windowFrame), kTrackingAreaHeight);
    trackingArea_.reset(
        [[CrTrackingArea alloc] initWithRect:trackingRect
                                     options:NSTrackingMouseEnteredAndExited |
                                             NSTrackingActiveAlways
                                       owner:self
                                    userInfo:nil]);
    [[[bwc window] contentView] addTrackingArea:trackingArea_.get()];

    // Install the Carbon event handler for the undocumented menu bar show/hide
    // event.
    EventTypeSpec eventSpec = { kEventClassMenu, 2004 };
    InstallApplicationEventHandler(NewEventHandlerUPP(&MenuBarRevealHandler),
                                   1, &eventSpec,
                                   self, &menuBarTrackingHandler_);
  }
  return self;
}

- (void)dealloc {
  RemoveEventHandler(menuBarTrackingHandler_);
  [[[controller_ window] contentView] removeTrackingArea:trackingArea_.get()];
  [super dealloc];
}

- (CGFloat)menuBarHeight {
  // -[NSMenu menuBarHeight] will return 0 when the menu bar is hidden, so
  // use the status bar API instead, which always returns a constant value.
  return [[NSStatusBar systemStatusBar] thickness] * menuBarRevealFraction_;
}

- (void)mouseEntered:(NSEvent*)event {
  if (animation_ || currentState_ == kFullscreenToolbarAndTabstrip)
    return;

  [self startAnimationToState:kFullscreenToolbarAndTabstrip];
}

- (void)mouseExited:(NSEvent*)event {
  if (animation_ || currentState_ == kFullscreenToolbarOnly)
    return;

  // Take allowance for the small gap between the window max Y and top edge of
  // the screen.
  NSPoint mousePoint = [NSEvent mouseLocation];
  NSRect screenRect = [[[controller_ window] screen] frame];
  if (mousePoint.y >= NSMaxY(screenRect) - kTrackingAreaMaxYEpsilon)
    return;

  [self startAnimationToState:kFullscreenToolbarOnly];
}

- (void)startAnimationToState:(FullscreenToolbarState)state {
  DCHECK_NE(currentState_, state);

  destinationState_ = state;

  animation_.reset([[FullscreenModeDropDownAnimation alloc]
      initWithFullscreenModeController:self]);
  [animation_ startAnimation];
}

- (void)setMenuBarRevealProgress:(CGFloat)progress {
  menuBarRevealFraction_ = progress;

  // If an animation is not running, then -layoutSubviews will not be called
  // for each tick of the menu bar reveal. Do that manually.
  // TODO(rsesek): This is kind of hacky and janky.
  if (!animation_)
    [controller_ layoutSubviews];
}

- (void)setRevealAnimationProgress:(NSAnimationProgress)progress {
  // When hiding the tabstrip, invert the fraction.
  if (destinationState_ == kFullscreenToolbarOnly)
    progress = 1.0 - progress;
  [controller_ setFloatingBarShownFraction:progress];
}

- (void)animationDidEnd:(NSAnimation*)animation {
  DCHECK_EQ(animation_.get(), animation);

  currentState_ = destinationState_;

  [animation_ setDelegate:nil];
  animation_.reset();
}

@end
