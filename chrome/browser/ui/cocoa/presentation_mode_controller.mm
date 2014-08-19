// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/presentation_mode_controller.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/mac/sdk_forward_declarations.h"
#import "base/mac/mac_util.h"
#include "chrome/browser/fullscreen.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/nsview_additions.h"
#include "chrome/common/chrome_switches.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMNSAnimation+Duration.h"

NSString* const kWillEnterFullscreenNotification =
    @"WillEnterFullscreenNotification";
NSString* const kWillLeaveFullscreenNotification =
    @"WillLeaveFullscreenNotification";

namespace {
// The activation zone for the main menu is 4 pixels high; if we make it any
// smaller, then the menu can be made to appear without the bar sliding down.
const CGFloat kDropdownActivationZoneHeight = 4;
const NSTimeInterval kDropdownAnimationDuration = 0.12;
const NSTimeInterval kMouseExitCheckDelay = 0.1;
// This show delay attempts to match the delay for the main menu.
const NSTimeInterval kDropdownShowDelay = 0.3;
const NSTimeInterval kDropdownHideDelay = 0.2;

// The amount by which the floating bar is offset downwards (to avoid the menu)
// in presentation mode. (We can't use |-[NSMenu menuBarHeight]| since it
// returns 0 when the menu bar is hidden.)
const CGFloat kFloatingBarVerticalOffset = 22;

}  // end namespace


// Helper class to manage animations for the dropdown bar.  Calls
// [PresentationModeController changeFloatingBarShownFraction] once per
// animation step.
@interface DropdownAnimation : NSAnimation {
 @private
  PresentationModeController* controller_;
  CGFloat startFraction_;
  CGFloat endFraction_;
}

@property(readonly, nonatomic) CGFloat startFraction;
@property(readonly, nonatomic) CGFloat endFraction;

// Designated initializer.  Asks |controller| for the current shown fraction, so
// if the bar is already partially shown or partially hidden, the animation
// duration may be less than |fullDuration|.
- (id)initWithFraction:(CGFloat)fromFraction
          fullDuration:(CGFloat)fullDuration
        animationCurve:(NSAnimationCurve)animationCurve
            controller:(PresentationModeController*)controller;

@end

@implementation DropdownAnimation

@synthesize startFraction = startFraction_;
@synthesize endFraction = endFraction_;

- (id)initWithFraction:(CGFloat)toFraction
          fullDuration:(CGFloat)fullDuration
        animationCurve:(NSAnimationCurve)animationCurve
            controller:(PresentationModeController*)controller {
  // Calculate the effective duration, based on the current shown fraction.
  DCHECK(controller);
  CGFloat fromFraction = [controller floatingBarShownFraction];
  CGFloat effectiveDuration = fabs(fullDuration * (fromFraction - toFraction));

  if ((self = [super gtm_initWithDuration:effectiveDuration
                                eventMask:NSLeftMouseDownMask
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
  [controller_ changeFloatingBarShownFraction:fraction];
}

@end


@interface PresentationModeController (PrivateMethods)

// Updates the visibility of the menu bar and the dock.
- (void)updateMenuBarAndDockVisibility;

// Whether the current screen is expected to have a menu bar, regardless of
// current visibility of the menu bar.
- (BOOL)doesScreenHaveMenuBar;

// Returns YES if the window is on the primary screen.
- (BOOL)isWindowOnPrimaryScreen;

// Returns |kFullScreenModeHideAll| when the overlay is hidden and
// |kFullScreenModeHideDock| when the overlay is shown.
- (base::mac::FullScreenMode)desiredSystemFullscreenMode;

// Change the overlay to the given fraction, with or without animation. Only
// guaranteed to work properly with |fraction == 0| or |fraction == 1|. This
// performs the show/hide (animation) immediately. It does not touch the timers.
- (void)changeOverlayToFraction:(CGFloat)fraction
                  withAnimation:(BOOL)animate;

// Schedule the floating bar to be shown/hidden because of mouse position.
- (void)scheduleShowForMouse;
- (void)scheduleHideForMouse;

// Set up the tracking area used to activate the sliding bar or keep it active
// using with the rectangle in |trackingAreaBounds_|, or remove the tracking
// area if one was previously set up.
- (void)setupTrackingArea;
- (void)removeTrackingAreaIfNecessary;

// Returns YES if the mouse is currently in any current tracking rectangle, NO
// otherwise.
- (BOOL)mouseInsideTrackingRect;

// The tracking area can "falsely" report exits when the menu slides down over
// it. In that case, we have to monitor for a "real" mouse exit on a timer.
// |-setupMouseExitCheck| schedules a check; |-cancelMouseExitCheck| cancels any
// scheduled check.
- (void)setupMouseExitCheck;
- (void)cancelMouseExitCheck;

// Called (after a delay) by |-setupMouseExitCheck|, to check whether the mouse
// has exited or not; if it hasn't, it will schedule another check.
- (void)checkForMouseExit;

// Start timers for showing/hiding the floating bar.
- (void)startShowTimer;
- (void)startHideTimer;
- (void)cancelShowTimer;
- (void)cancelHideTimer;
- (void)cancelAllTimers;

// Methods called when the show/hide timers fire. Do not call directly.
- (void)showTimerFire:(NSTimer*)timer;
- (void)hideTimerFire:(NSTimer*)timer;

// Stops any running animations, removes tracking areas, etc.
- (void)cleanup;

// Shows and hides the UI associated with this window being active (having main
// status).  This includes hiding the menu bar.  These functions are called when
// the window gains or loses main status as well as in |-cleanup|.
- (void)showActiveWindowUI;
- (void)hideActiveWindowUI;

@end


@implementation PresentationModeController

@synthesize inPresentationMode = inPresentationMode_;

- (id)initWithBrowserController:(BrowserWindowController*)controller {
  if ((self = [super init])) {
    browserController_ = controller;
    systemFullscreenMode_ = base::mac::kFullScreenModeNormal;
  }

  // Let the world know what we're up to.
  [[NSNotificationCenter defaultCenter]
    postNotificationName:kWillEnterFullscreenNotification
                  object:nil];

  return self;
}

- (void)dealloc {
  DCHECK(!inPresentationMode_);
  DCHECK(!trackingArea_);
  [super dealloc];
}

- (void)enterPresentationModeForContentView:(NSView*)contentView
                               showDropdown:(BOOL)showDropdown {
  DCHECK(!inPresentationMode_);
  enteringPresentationMode_ = YES;
  inPresentationMode_ = YES;
  contentView_ = contentView;
  [self changeFloatingBarShownFraction:(showDropdown ? 1 : 0)];

  // Register for notifications.  Self is removed as an observer in |-cleanup|.
  NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
  NSWindow* window = [browserController_ window];

  // Disable these notifications on Lion as they cause crashes.
  // TODO(rohitrao): Figure out what happens if a fullscreen window changes
  // monitors on Lion.
  if (base::mac::IsOSSnowLeopard()) {
    [nc addObserver:self
           selector:@selector(windowDidChangeScreen:)
               name:NSWindowDidChangeScreenNotification
             object:window];

    [nc addObserver:self
           selector:@selector(windowDidMove:)
               name:NSWindowDidMoveNotification
             object:window];
  }

  [nc addObserver:self
         selector:@selector(windowDidBecomeMain:)
             name:NSWindowDidBecomeMainNotification
           object:window];

  [nc addObserver:self
         selector:@selector(windowDidResignMain:)
             name:NSWindowDidResignMainNotification
           object:window];

  enteringPresentationMode_ = NO;
}

- (void)exitPresentationMode {
  [[NSNotificationCenter defaultCenter]
    postNotificationName:kWillLeaveFullscreenNotification
                  object:nil];
  DCHECK(inPresentationMode_);
  inPresentationMode_ = NO;

  [self cleanup];
}

- (void)windowDidChangeScreen:(NSNotification*)notification {
  [browserController_ resizeFullscreenWindow];
}

- (void)windowDidMove:(NSNotification*)notification {
  [browserController_ resizeFullscreenWindow];
}

- (void)windowDidBecomeMain:(NSNotification*)notification {
  [self showActiveWindowUI];
}

- (void)windowDidResignMain:(NSNotification*)notification {
  [self hideActiveWindowUI];
}

// On OSX 10.8+, the menu bar shows on the secondary screen in fullscreen.
// On OSX 10.7, fullscreen never fills the secondary screen.
// On OSX 10.6, the menu bar never shows on the secondary screen in fullscreen.
// See http://crbug.com/388906 for full details.
- (CGFloat)floatingBarVerticalOffset {
  if (base::mac::IsOSMountainLionOrLater())
    return kFloatingBarVerticalOffset;
  return [self isWindowOnPrimaryScreen] ? kFloatingBarVerticalOffset : 0;
}

- (void)overlayFrameChanged:(NSRect)frame {
  if (!inPresentationMode_)
    return;

  // Make sure |trackingAreaBounds_| always reflects either the tracking area or
  // the desired tracking area.
  trackingAreaBounds_ = frame;
  // The tracking area should always be at least the height of activation zone.
  NSRect contentBounds = [contentView_ bounds];
  trackingAreaBounds_.origin.y =
      std::min(trackingAreaBounds_.origin.y,
               NSMaxY(contentBounds) - kDropdownActivationZoneHeight);
  trackingAreaBounds_.size.height =
      NSMaxY(contentBounds) - trackingAreaBounds_.origin.y + 1;

  // If an animation is currently running, do not set up a tracking area now.
  // Instead, leave it to be created it in |-animationDidEnd:|.
  if (currentAnimation_)
    return;

  // If this is part of the initial setup, lock bar visibility if the mouse is
  // within the tracking area bounds.
  if (enteringPresentationMode_ && [self mouseInsideTrackingRect])
    [browserController_ lockBarVisibilityForOwner:self
                                    withAnimation:NO
                                            delay:NO];
  [self setupTrackingArea];
}

- (void)ensureOverlayShownWithAnimation:(BOOL)animate delay:(BOOL)delay {
  if (!inPresentationMode_)
    return;

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode))
    return;

  if (animate) {
    if (delay) {
      [self startShowTimer];
    } else {
      [self cancelAllTimers];
      [self changeOverlayToFraction:1 withAnimation:YES];
    }
  } else {
    DCHECK(!delay);
    [self cancelAllTimers];
    [self changeOverlayToFraction:1 withAnimation:NO];
  }
}

- (void)ensureOverlayHiddenWithAnimation:(BOOL)animate delay:(BOOL)delay {
  if (!inPresentationMode_)
    return;

  if (animate) {
    if (delay) {
      [self startHideTimer];
    } else {
      [self cancelAllTimers];
      [self changeOverlayToFraction:0 withAnimation:YES];
    }
  } else {
    DCHECK(!delay);
    [self cancelAllTimers];
    [self changeOverlayToFraction:0 withAnimation:NO];
  }
}

- (void)cancelAnimationAndTimers {
  [self cancelAllTimers];
  [currentAnimation_ stopAnimation];
  currentAnimation_.reset();
}

- (CGFloat)floatingBarShownFraction {
  return [browserController_ floatingBarShownFraction];
}

- (void)setSystemFullscreenModeTo:(base::mac::FullScreenMode)mode {
  if (mode == systemFullscreenMode_)
    return;
  if (systemFullscreenMode_ == base::mac::kFullScreenModeNormal)
    base::mac::RequestFullScreen(mode);
  else if (mode == base::mac::kFullScreenModeNormal)
    base::mac::ReleaseFullScreen(systemFullscreenMode_);
  else
    base::mac::SwitchFullScreenModes(systemFullscreenMode_, mode);
  systemFullscreenMode_ = mode;
}

- (void)changeFloatingBarShownFraction:(CGFloat)fraction {
  [browserController_ setFloatingBarShownFraction:fraction];

  [self updateMenuBarAndDockVisibility];
}

// Used to activate the floating bar in presentation mode.
- (void)mouseEntered:(NSEvent*)event {
  DCHECK(inPresentationMode_);

  // Having gotten a mouse entered, we no longer need to do exit checks.
  [self cancelMouseExitCheck];

  NSTrackingArea* trackingArea = [event trackingArea];
  if (trackingArea == trackingArea_) {
    // The tracking area shouldn't be active during animation.
    DCHECK(!currentAnimation_);
    [self scheduleShowForMouse];
  }
}

// Used to deactivate the floating bar in presentation mode.
- (void)mouseExited:(NSEvent*)event {
  DCHECK(inPresentationMode_);

  NSTrackingArea* trackingArea = [event trackingArea];
  if (trackingArea == trackingArea_) {
    // The tracking area shouldn't be active during animation.
    DCHECK(!currentAnimation_);

    // We can get a false mouse exit when the menu slides down, so if the mouse
    // is still actually over the tracking area, we ignore the mouse exit, but
    // we set up to check the mouse position again after a delay.
    if ([self mouseInsideTrackingRect]) {
      [self setupMouseExitCheck];
      return;
    }

    [self scheduleHideForMouse];
  }
}

- (void)animationDidStop:(NSAnimation*)animation {
  // Reset the |currentAnimation_| pointer now that the animation is over.
  currentAnimation_.reset();

  // Invariant says that the tracking area is not installed while animations are
  // in progress. Ensure this is true.
  DCHECK(!trackingArea_);
  [self removeTrackingAreaIfNecessary];  // For paranoia.

  // Don't automatically set up a new tracking area. When explicitly stopped,
  // either another animation is going to start immediately or the state will be
  // changed immediately.
}

- (void)animationDidEnd:(NSAnimation*)animation {
  [self animationDidStop:animation];

  // |trackingAreaBounds_| contains the correct tracking area bounds, including
  // |any updates that may have come while the animation was running. Install a
  // new tracking area with these bounds.
  [self setupTrackingArea];

  // TODO(viettrungluu): Better would be to check during the animation; doing it
  // here means that the timing is slightly off.
  if (![self mouseInsideTrackingRect])
    [self scheduleHideForMouse];
}

@end


@implementation PresentationModeController (PrivateMethods)

- (void)updateMenuBarAndDockVisibility {
  if (![[browserController_ window] isMainWindow] ||
      ![browserController_ isInImmersiveFullscreen]) {
    [self setSystemFullscreenModeTo:base::mac::kFullScreenModeNormal];
    return;
  }

  // The screen does not have a menu bar, so there's no need to hide it.
  if (![self doesScreenHaveMenuBar]) {
    [self setSystemFullscreenModeTo:base::mac::kFullScreenModeHideDock];
    return;
  }

  [self setSystemFullscreenModeTo:[self desiredSystemFullscreenMode]];
}

- (BOOL)doesScreenHaveMenuBar {
  if (![[NSScreen class]
          respondsToSelector:@selector(screensHaveSeparateSpaces)])
    return [self isWindowOnPrimaryScreen];

  BOOL eachScreenShouldHaveMenuBar = [NSScreen screensHaveSeparateSpaces];
  return eachScreenShouldHaveMenuBar ?: [self isWindowOnPrimaryScreen];
}

- (BOOL)isWindowOnPrimaryScreen {
  NSScreen* screen = [[browserController_ window] screen];
  NSScreen* primaryScreen = [[NSScreen screens] objectAtIndex:0];
  return (screen == primaryScreen);
}

- (base::mac::FullScreenMode)desiredSystemFullscreenMode {
  if ([browserController_ floatingBarShownFraction] >= 1.0)
    return base::mac::kFullScreenModeHideDock;
  return base::mac::kFullScreenModeHideAll;
}

- (void)changeOverlayToFraction:(CGFloat)fraction
                  withAnimation:(BOOL)animate {
  // The non-animated case is really simple, so do it and return.
  if (!animate) {
    [currentAnimation_ stopAnimation];
    [self changeFloatingBarShownFraction:fraction];
    return;
  }

  // If we're already animating to the given fraction, then there's nothing more
  // to do.
  if (currentAnimation_ && [currentAnimation_ endFraction] == fraction)
    return;

  // In all other cases, we want to cancel any running animation (which may be
  // to show or to hide).
  [currentAnimation_ stopAnimation];

  // Now, if it happens to already be in the right state, there's nothing more
  // to do.
  if ([browserController_ floatingBarShownFraction] == fraction)
    return;

  // Create the animation and set it up.
  currentAnimation_.reset(
      [[DropdownAnimation alloc] initWithFraction:fraction
                                     fullDuration:kDropdownAnimationDuration
                                   animationCurve:NSAnimationEaseOut
                                       controller:self]);
  DCHECK(currentAnimation_);
  [currentAnimation_ setAnimationBlockingMode:NSAnimationNonblocking];
  [currentAnimation_ setDelegate:self];

  // If there is an existing tracking area, remove it. We do not track mouse
  // movements during animations (see class comment in the header file).
  [self removeTrackingAreaIfNecessary];

  [currentAnimation_ startAnimation];
}

- (void)scheduleShowForMouse {
  [browserController_ lockBarVisibilityForOwner:self
                                  withAnimation:YES
                                          delay:YES];
}

- (void)scheduleHideForMouse {
  [browserController_ releaseBarVisibilityForOwner:self
                                     withAnimation:YES
                                             delay:YES];
}

- (void)setupTrackingArea {
  if (trackingArea_) {
    // If the tracking rectangle is already |trackingAreaBounds_|, quit early.
    NSRect oldRect = [trackingArea_ rect];
    if (NSEqualRects(trackingAreaBounds_, oldRect))
      return;

    // Otherwise, remove it.
    [self removeTrackingAreaIfNecessary];
  }

  // Create and add a new tracking area for |frame|.
  trackingArea_.reset(
      [[NSTrackingArea alloc] initWithRect:trackingAreaBounds_
                                   options:NSTrackingMouseEnteredAndExited |
                                           NSTrackingActiveInKeyWindow
                                     owner:self
                                  userInfo:nil]);
  DCHECK(contentView_);
  [contentView_ addTrackingArea:trackingArea_];
}

- (void)removeTrackingAreaIfNecessary {
  if (trackingArea_) {
    DCHECK(contentView_);  // |contentView_| better be valid.
    [contentView_ removeTrackingArea:trackingArea_];
    trackingArea_.reset();
  }
}

- (BOOL)mouseInsideTrackingRect {
  NSWindow* window = [browserController_ window];
  NSPoint mouseLoc = [window mouseLocationOutsideOfEventStream];
  NSPoint mousePos = [contentView_ convertPoint:mouseLoc fromView:nil];
  return NSMouseInRect(mousePos, trackingAreaBounds_, [contentView_ isFlipped]);
}

- (void)setupMouseExitCheck {
  [self performSelector:@selector(checkForMouseExit)
             withObject:nil
             afterDelay:kMouseExitCheckDelay];
}

- (void)cancelMouseExitCheck {
  [NSObject cancelPreviousPerformRequestsWithTarget:self
      selector:@selector(checkForMouseExit) object:nil];
}

- (void)checkForMouseExit {
  if ([self mouseInsideTrackingRect])
    [self setupMouseExitCheck];
  else
    [self scheduleHideForMouse];
}

- (void)startShowTimer {
  // If there's already a show timer going, just keep it.
  if (showTimer_) {
    DCHECK([showTimer_ isValid]);
    DCHECK(!hideTimer_);
    return;
  }

  // Cancel the hide timer (if necessary) and set up the new show timer.
  [self cancelHideTimer];
  showTimer_.reset(
      [[NSTimer scheduledTimerWithTimeInterval:kDropdownShowDelay
                                        target:self
                                      selector:@selector(showTimerFire:)
                                      userInfo:nil
                                       repeats:NO] retain]);
  DCHECK([showTimer_ isValid]);  // This also checks that |showTimer_ != nil|.
}

- (void)startHideTimer {
  // If there's already a hide timer going, just keep it.
  if (hideTimer_) {
    DCHECK([hideTimer_ isValid]);
    DCHECK(!showTimer_);
    return;
  }

  // Cancel the show timer (if necessary) and set up the new hide timer.
  [self cancelShowTimer];
  hideTimer_.reset(
      [[NSTimer scheduledTimerWithTimeInterval:kDropdownHideDelay
                                        target:self
                                      selector:@selector(hideTimerFire:)
                                      userInfo:nil
                                       repeats:NO] retain]);
  DCHECK([hideTimer_ isValid]);  // This also checks that |hideTimer_ != nil|.
}

- (void)cancelShowTimer {
  [showTimer_ invalidate];
  showTimer_.reset();
}

- (void)cancelHideTimer {
  [hideTimer_ invalidate];
  hideTimer_.reset();
}

- (void)cancelAllTimers {
  [self cancelShowTimer];
  [self cancelHideTimer];
}

- (void)showTimerFire:(NSTimer*)timer {
  DCHECK_EQ(showTimer_, timer);  // This better be our show timer.
  [showTimer_ invalidate];       // Make sure it doesn't repeat.
  showTimer_.reset();            // And get rid of it.
  [self changeOverlayToFraction:1 withAnimation:YES];
}

- (void)hideTimerFire:(NSTimer*)timer {
  DCHECK_EQ(hideTimer_, timer);  // This better be our hide timer.
  [hideTimer_ invalidate];       // Make sure it doesn't repeat.
  hideTimer_.reset();            // And get rid of it.
  [self changeOverlayToFraction:0 withAnimation:YES];
}

- (void)cleanup {
  [self cancelMouseExitCheck];
  [self cancelAnimationAndTimers];
  [[NSNotificationCenter defaultCenter] removeObserver:self];

  [self removeTrackingAreaIfNecessary];
  contentView_ = nil;

  // This isn't tracked when not in presentation mode.
  [browserController_ releaseBarVisibilityForOwner:self
                                     withAnimation:NO
                                             delay:NO];

  // Call the main status resignation code to perform the associated cleanup,
  // since we will no longer be receiving actual status resignation
  // notifications.
  [self setSystemFullscreenModeTo:base::mac::kFullScreenModeNormal];

  // No more calls back up to the BWC.
  browserController_ = nil;
}

- (void)showActiveWindowUI {
  [self updateMenuBarAndDockVisibility];

  // TODO(rohitrao): Insert the Exit Fullscreen button.  http://crbug.com/35956
}

- (void)hideActiveWindowUI {
  [self updateMenuBarAndDockVisibility];

  // TODO(rohitrao): Remove the Exit Fullscreen button.  http://crbug.com/35956
}

@end
