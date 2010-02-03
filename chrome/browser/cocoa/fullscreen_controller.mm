// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/fullscreen_controller.h"

#import "chrome/browser/cocoa/browser_window_controller.h"
#import "third_party/GTM/AppKit/GTMNSAnimation+Duration.h"


namespace {
const CGFloat kDropdownAnimationDuration = 0.12;
const CGFloat kDropdownShowDelay = 0.2;
const CGFloat kDropdownHideDelay = 0.2;
}  // end namespace


// Helper class to manage animations for the fullscreen dropdown bar.  Calls
// back to [BrowserWindowController setFloatingBarShownFraction] once per
// animation step.
@interface DropdownAnimation : NSAnimation {
  BrowserWindowController* controller_;
  CGFloat startFraction_;
  CGFloat endFraction_;
}
// Designated initializer.  Asks |controller| for the current shown fraction, so
// if the bar is already partially shown or partially hidden, the animation
// duration may be less than |fullDuration|.
- (id)initWithFraction:(CGFloat)fromFraction
          fullDuration:(CGFloat)fullDuration
        animationCurve:(NSInteger)animationCurve
            controller:(BrowserWindowController*)controller;
@end

@implementation DropdownAnimation
- (id)initWithFraction:(CGFloat)toFraction
          fullDuration:(CGFloat)fullDuration
        animationCurve:(NSInteger)animationCurve
            controller:(BrowserWindowController*)controller {
  // Calculate the effective duration, based on the current shown fraction.
  DCHECK(controller);
  CGFloat fromFraction = [controller floatingBarShownFraction];
  CGFloat effectiveDuration = fabs(fullDuration * (fromFraction - toFraction));

  if ((self = [super gtm_initWithDuration:effectiveDuration
                           animationCurve:animationCurve])) {
    startFraction_ = fromFraction;
    endFraction_ = toFraction;
    controller_ = controller;
  }
  return self;
}

// Called once per animation step.  Overridden to change the floating bar's
// position based on the animation's progress.
- (void)setCurrentProgress:(NSAnimationProgress)progress {
  CGFloat fraction =
      startFraction_ + (progress * (endFraction_ - startFraction_));
  [controller_ setFloatingBarShownFraction:fraction];
}
@end


@interface FullscreenController (PrivateMethods)
// Shows or hides the overlay, with animation.  The overlay cannot be hidden if
// any of its "subviews" currently has focus or if the mouse is currently over
// the overlay.
- (void)showOverlay;
- (void)hideOverlayIfPossible;

// Stops any running animations, removes tracking areas, etc.  Common cleanup
// code shared by exitFullscreen: and dealloc:.
- (void)cleanup;
@end


@implementation FullscreenController

- (id)initWithBrowserController:(BrowserWindowController*)controller {
  if ((self == [super init])) {
    browserController_ = controller;
  }
  return self;
}

- (void)dealloc {
  // Perform all the exit fullscreen steps, including stopping any running
  // animations and cleaning up tracking areas.
  [self cleanup];
  [super dealloc];
}

- (void)enterFullscreenForContentView:(NSView*)contentView
                         showDropdown:(BOOL)showDropdown {
  DCHECK(!isFullscreen_);
  isFullscreen_ = YES;
  contentView_ = contentView;
  [browserController_ setFloatingBarShownFraction:(showDropdown ? 1 : 0)];

  // Since there is no way to get a pointer to the correct LocationBarViewMac to
  // pass as the |object| parameter, we register for all location bar focus
  // notifications and filter for those from the same window once the
  // notification is received.
  NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
  [nc addObserver:self
         selector:@selector(gainedFocus:)
             name:kLocationBarGainedFocusNotification
           object:nil];
  [nc addObserver:self
         selector:@selector(lostFocus:)
             name:kLocationBarLostFocusNotification
           object:nil];
}

- (void)exitFullscreen {
  DCHECK(isFullscreen_);
  [self cleanup];
  isFullscreen_ = NO;
}

- (void)overlayFrameChanged:(NSRect)frame {
  if (!isFullscreen_)
    return;

  if (!NSIntersectsRect(frame, [contentView_ bounds])) {
    // Reset the tracking area to be a single pixel high at the top border of
    // the content view.
    frame = [contentView_ bounds];
    frame.origin.y = NSMaxY(frame) - 1;
    frame.size.height = 1;
  }

  // If an animation is currently running, do not set up a tracking area.
  // Instead, save the bounds and we will create it in animationDidEnd:.
  if (currentAnimation_.get()) {
    trackingAreaBounds_ = frame;
    return;
  }

  NSWindow* window = [browserController_ window];
  NSPoint mouseLoc =
      [contentView_ convertPointFromBase:
                      [window mouseLocationOutsideOfEventStream]];

  if (trackingArea_.get()) {
    // If the tracking rectangle is already |rect|, quit early.
    NSRect oldRect = [trackingArea_ rect];
    if (NSEqualRects(frame, oldRect))
      return;

    // Otherwise, remove it.
    [contentView_ removeTrackingArea:trackingArea_.get()];
  }

  // Create and add a new tracking area for |frame|.
  trackingArea_.reset(
      [[NSTrackingArea alloc] initWithRect:frame
                                   options:NSTrackingMouseEnteredAndExited |
                                           NSTrackingActiveInKeyWindow
                                     owner:self
                                  userInfo:nil]);
  [contentView_ addTrackingArea:trackingArea_.get()];
}

- (void)gainedFocus:(NSNotification*)notification {
  LocationBarViewMac* bar =
      static_cast<LocationBarViewMac*>([[notification object] pointerValue]);
  if ([bar->location_entry()->GetNativeView() window] ==
      [browserController_ window]) {
    [self showOverlay];
  }
}

- (void)lostFocus:(NSNotification*)notification {
  LocationBarViewMac* bar =
      static_cast<LocationBarViewMac*>([[notification object] pointerValue]);
  if ([bar->location_entry()->GetNativeView() window] ==
      [browserController_ window]) {
    [self hideOverlayIfPossible];
  }
}

// Used to activate the floating bar in fullscreen mode.
- (void)mouseEntered:(NSEvent*)event {
  DCHECK(isFullscreen_);

  NSTrackingArea* trackingArea = [event trackingArea];
  if (trackingArea == trackingArea_.get()) {
    // Cancel any pending hides and set the bar to show after a delay.
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    if (currentAnimation_.get()) {
      [self showOverlay];
    } else {
      [self performSelector:@selector(showOverlay)
                 withObject:nil
                 afterDelay:kDropdownShowDelay];
    }
  }
}

// Used to deactivate the floating bar in fullscreen mode.
- (void)mouseExited:(NSEvent*)event {
  DCHECK(isFullscreen_);

  NSTrackingArea* trackingArea = [event trackingArea];
  if (trackingArea == trackingArea_.get()) {
    // We can get a false mouse exit when the menu slides down, so if the mouse
    // is where the menu would be, ignore the mouse exit.
    CGFloat mainMenuHeight = [[NSApp mainMenu] menuBarHeight];
    NSWindow* window = [browserController_ window];
    NSPoint mouseLoc = [window mouseLocationOutsideOfEventStream];
    if (mouseLoc.y >= NSHeight([window frame]) - mainMenuHeight)
      return;

    // Cancel any pending shows and set the bar to hide after a delay.
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    if (currentAnimation_.get()) {
      [self hideOverlayIfPossible];
    } else {
      [self performSelector:@selector(hideOverlayIfPossible)
                 withObject:nil
                 afterDelay:kDropdownHideDelay];
    }
  }
}

- (void)animationDidEnd:(NSAnimation*)animation {
  // Reset the |currentAnimation_| pointer now that the animation is finished.
  currentAnimation_.reset(nil);

  // Invariant says that the tracking area is not installed while animations are
  // in progress.  Check to make sure this is true.
  DCHECK(!trackingArea_.get());
  if (trackingArea_.get())
    [contentView_ removeTrackingArea:trackingArea_.get()];

  // |trackingAreaBounds_| contains the correct tracking area bounds, including
  // |any updates that may have come while the animation was running.  Install a
  // |new tracking area with these bounds.
  trackingArea_.reset(
      [[NSTrackingArea alloc] initWithRect:trackingAreaBounds_
                                   options:NSTrackingMouseEnteredAndExited |
                                           NSTrackingActiveInKeyWindow
                                     owner:self
                                  userInfo:nil]);
  [contentView_ addTrackingArea:trackingArea_.get()];
}

@end


@implementation FullscreenController (PrivateMethods)

- (void)showOverlay {
  [NSObject cancelPreviousPerformRequestsWithTarget:self];

  // This method can be called by LocationBarViewMac even when we're not in
  // fullscreen mode.  Do nothing in that case.
  if (!isFullscreen_)
    return;

  // Show the overlay with animation, first stopping any running animations.
  if (currentAnimation_.get())
    [currentAnimation_ stopAnimation];
  currentAnimation_.reset(
      [[DropdownAnimation alloc] initWithFraction:1
                                     fullDuration:kDropdownAnimationDuration
                                   animationCurve:NSAnimationEaseIn
                                       controller:browserController_]);
  [currentAnimation_ setAnimationBlockingMode:NSAnimationNonblocking];
  [currentAnimation_ setDelegate:self];

  // If there is an existing tracking area, remove it.  We do not track mouse
  // movements during animations (see class comment in the .h file).
  if (currentAnimation_ && trackingArea_.get()) {
    [contentView_ removeTrackingArea:trackingArea_.get()];
    trackingArea_.reset(nil);
  }
  [currentAnimation_ startAnimation];
}

- (void)hideOverlayIfPossible {
  [NSObject cancelPreviousPerformRequestsWithTarget:self];

  if (!isFullscreen_)
    return;

  NSWindow* window = [browserController_ window];

  // If the floating bar currently has focus, do not hide the overlay.
  if ([browserController_ floatingBarHasFocus])
    return;

  // We can get a false mouse exit when the menu slides down, so if the mouse is
  // where the menu would be, ignore the mouse exit.
  // TODO(rohitrao): Set a timer to check where the mouse is later, to make
  // sure the floating bar doesn't get stuck.
  CGFloat mainMenuHeight = [[NSApp mainMenu] menuBarHeight];
  NSPoint mouseLoc = [window mouseLocationOutsideOfEventStream];
  if (mouseLoc.y >= NSHeight([window frame]) - mainMenuHeight)
    return;

  // Do not hide the bar if the mouse is currently over the dropdown view.  We
  // use the current tracking area as a proxy for the dropdown's location.  If
  // there is no current tracking area (possible because we currently do not
  // always open the dropdown when the omnibox gets focus), then go ahead and
  // close the dropdown.
  if (trackingArea_.get() &&
      NSMouseInRect([contentView_ convertPointFromBase:mouseLoc],
                    [trackingArea_ rect],
                    [contentView_ isFlipped])) {
    return;
  }

  // Hide the overlay with animation, first stopping any running animations.
  if (currentAnimation_.get())
    [currentAnimation_ stopAnimation];
  currentAnimation_.reset(
      [[DropdownAnimation alloc] initWithFraction:0
                                     fullDuration:kDropdownAnimationDuration
                                   animationCurve:NSAnimationEaseOut
                                       controller:browserController_]);
  [currentAnimation_ setAnimationBlockingMode:NSAnimationNonblocking];
  [currentAnimation_ setDelegate:self];

  // If there is an existing tracking area, remove it.  We do not track mouse
  // movements during animations.
  if (currentAnimation_ && trackingArea_.get()) {
    [contentView_ removeTrackingArea:trackingArea_.get()];
    trackingArea_.reset(nil);
  }

  [currentAnimation_ startAnimation];
}

- (void)cleanup {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  [currentAnimation_ stopAnimation];
  [contentView_ removeTrackingArea:trackingArea_.get()];
  contentView_ = nil;
  currentAnimation_.reset(nil);
  trackingArea_.reset(nil);
}

@end
