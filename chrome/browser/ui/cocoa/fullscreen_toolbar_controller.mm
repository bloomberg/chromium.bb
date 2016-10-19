// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/fullscreen_toolbar_controller.h"

#include <algorithm>

#import "base/auto_reset.h"
#include "base/command_line.h"
#import "base/mac/mac_util.h"
#include "base/mac/sdk_forward_declarations.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_menubar_tracker.h"
#include "chrome/common/chrome_switches.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMNSAnimation+Duration.h"
#include "ui/base/cocoa/appkit_utils.h"
#import "ui/base/cocoa/nsview_additions.h"
#import "ui/base/cocoa/tracking_area.h"

namespace {

// The duration of the toolbar show/hide animation.
const NSTimeInterval kDropdownAnimationDuration = 0.20;

// If the fullscreen toolbar is hidden, it is difficult for the user to see
// changes in the tabstrip. As a result, if a tab is inserted or the current
// tab switched to a new one, the toolbar must animate in and out to display
// the tabstrip changes to the user. The animation drops down the toolbar and
// then wait for 0.75 seconds before it hides the toolbar.
const NSTimeInterval kTabStripChangesDelay = 0.75;

// Additional height threshold added at the toolbar's bottom. This is to mimic
// threshold the mouse position needs to be at before the menubar automatically
// hides.
const CGFloat kTrackingAreaAdditionalThreshold = 20;

// Visibility fractions for the menubar and toolbar.
const CGFloat kHideFraction = 0.0;
const CGFloat kShowFraction = 1.0;

// The amount by which the toolbar is offset downwards (to avoid the menu)
// when the toolbar style is OMNIBOX_TABS_HIDDEN. (We can't use
// |-[NSMenu menuBarHeight]| since it returns 0 when the menu bar is hidden.)
const CGFloat kToolbarVerticalOffset = 22;

}  // end namespace

// Helper class to manage animations for the dropdown bar.  Calls
// [FullscreenToolbarController changeToolbarFraction] once per
// animation step.
@interface DropdownAnimation : NSAnimation {
 @private
  FullscreenToolbarController* controller_;
  CGFloat startFraction_;
  CGFloat endFraction_;
  CGFloat toolbarFraction_;
}

@property(readonly, nonatomic) CGFloat endFraction;
@property(readonly, nonatomic) CGFloat toolbarFraction;

// Designated initializer.  Asks |controller| for the current shown fraction, so
// if the bar is already partially shown or partially hidden, the animation
// duration may be less than |fullDuration|.
- (id)initWithFraction:(CGFloat)fromFraction
          fullDuration:(CGFloat)fullDuration
        animationCurve:(NSAnimationCurve)animationCurve
            controller:(FullscreenToolbarController*)controller;

@end

@implementation DropdownAnimation

@synthesize endFraction = endFraction_;
@synthesize toolbarFraction = toolbarFraction_;

- (id)initWithFraction:(CGFloat)toFraction
          fullDuration:(CGFloat)fullDuration
        animationCurve:(NSAnimationCurve)animationCurve
            controller:(FullscreenToolbarController*)controller {
  // Calculate the effective duration, based on the current shown fraction.
  DCHECK(controller);
  CGFloat fromFraction = [controller toolbarFraction];
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
  toolbarFraction_ =
      startFraction_ + (progress * (endFraction_ - startFraction_));
  [controller_ updateToolbar];
}

@end

@interface FullscreenToolbarController (PrivateMethods)

// Updates the visibility of the menu bar and the dock.
- (void)updateMenuBarAndDockVisibility;

// Methods to set up or remove the tracking area.
- (void)updateTrackingArea;
- (void)removeTrackingAreaIfNecessary;

// Returns YES if the mouse is inside the tracking area.
- (BOOL)mouseInsideTrackingArea;

// Whether the current screen is expected to have a menu bar, regardless of
// current visibility of the menu bar.
- (BOOL)doesScreenHaveMenuBar;

// Returns YES if the window is on the primary screen.
- (BOOL)isWindowOnPrimaryScreen;

// Returns |kFullScreenModeHideAll| when the overlay is hidden and
// |kFullScreenModeHideDock| when the overlay is shown.
- (base::mac::FullScreenMode)desiredSystemFullscreenMode;

// Animate the overlay to the given visibility with animation. If |visible|
// is true, animate the toolbar to a fraction of 1.0. Otherwise it's 0.0.
- (void)animateToolbarVisibility:(BOOL)visible;

// Cancels the timer for hiding the floating bar.
- (void)cancelHideTimer;

// Methods called when the hide timers fire. Do not call directly.
- (void)hideTimerFire:(NSTimer*)timer;

// Stops any running animations, etc.
- (void)cleanup;

// Whether the menu bar should be shown in immersive fullscreen for the screen
// that contains the window.
- (BOOL)shouldShowMenubarInImmersiveFullscreen;

@end

@implementation FullscreenToolbarController

@synthesize slidingStyle = slidingStyle_;

- (id)initWithBrowserController:(BrowserWindowController*)controller
                          style:(FullscreenSlidingStyle)style {
  if ((self = [super init])) {
    browserController_ = controller;
    systemFullscreenMode_ = base::mac::kFullScreenModeNormal;
    slidingStyle_ = style;
  }

  return self;
}

- (void)dealloc {
  DCHECK(!inFullscreenMode_);
  [super dealloc];
}

- (void)setupFullscreenToolbarForContentView:(NSView*)contentView {
  DCHECK(!inFullscreenMode_);
  contentView_ = contentView;
  inFullscreenMode_ = YES;

  menubarTracker_.reset([[FullscreenMenubarTracker alloc]
      initWithFullscreenToolbarController:self]);

  [self updateMenuBarAndDockVisibility];

  // Register for notifications.  Self is removed as an observer in |-cleanup|.
  NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
  NSWindow* window = [browserController_ window];

  [nc addObserver:self
         selector:@selector(windowDidBecomeMain:)
             name:NSWindowDidBecomeMainNotification
           object:window];

  [nc addObserver:self
         selector:@selector(windowDidResignMain:)
             name:NSWindowDidResignMainNotification
           object:window];
}

- (void)exitFullscreenMode {
  DCHECK(inFullscreenMode_);
  inFullscreenMode_ = NO;

  [self cleanup];
}

- (void)windowDidChangeScreen:(NSNotification*)notification {
  [browserController_ resizeFullscreenWindow];
}

- (void)windowDidMove:(NSNotification*)notification {
  [browserController_ resizeFullscreenWindow];
}

- (void)windowDidBecomeMain:(NSNotification*)notification {
  [self updateMenuBarAndDockVisibility];
}

- (void)windowDidResignMain:(NSNotification*)notification {
  [self updateMenuBarAndDockVisibility];
}

- (void)lockBarVisibilityWithAnimation:(BOOL)animate {
  base::AutoReset<BOOL> autoReset(&isLockingBarVisibility_, YES);
  [self ensureOverlayShownWithAnimation:animate];
}

- (void)releaseBarVisibilityWithAnimation:(BOOL)animate {
  base::AutoReset<BOOL> autoReset(&isReleasingBarVisibility_, YES);
  [self ensureOverlayHiddenWithAnimation:animate];
}

- (void)ensureOverlayShownWithAnimation:(BOOL)animate {
  if (!inFullscreenMode_)
    return;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode))
    return;

  if (self.slidingStyle != FullscreenSlidingStyle::OMNIBOX_TABS_HIDDEN)
    return;

  [self cancelHideTimer];
  [self animateToolbarVisibility:YES];
}

- (void)ensureOverlayHiddenWithAnimation:(BOOL)animate {
  if (!inFullscreenMode_)
    return;

  if (self.slidingStyle != FullscreenSlidingStyle::OMNIBOX_TABS_HIDDEN)
    return;

  if ([browserController_ isBarVisibilityLockedForOwner:nil])
    return;

  if ([self mouseInsideTrackingArea] ||
      [menubarTracker_ state] == FullscreenMenubarState::SHOWN) {
    return;
  }

  [self cancelHideTimer];
  [self animateToolbarVisibility:NO];
}

- (void)cancelAnimationAndTimer {
  [self cancelHideTimer];
  [currentAnimation_ stopAnimation];
  currentAnimation_.reset();
}

- (void)revealToolbarForTabStripChanges {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableFullscreenToolbarReveal)) {
    return;
  }

  // Reveal the toolbar for tabstrip changes if the toolbar is hidden.
  if (ui::IsCGFloatEqual([self toolbarFraction], kHideFraction)) {
    isRevealingToolbarForTabStripChanges_ = YES;
    [self ensureOverlayShownWithAnimation:YES];
  }
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

- (void)mouseEntered:(NSEvent*)event {
  // Empty implementation. Required for CrTrackingArea.
}

- (void)mouseExited:(NSEvent*)event {
  DCHECK(inFullscreenMode_);
  DCHECK_EQ([event trackingArea], trackingArea_.get());

  if ([browserController_ isBarVisibilityLockedForOwner:nil])
    return;

  // If the menubar is gone, animate the toolbar out.
  if ([menubarTracker_ state] == FullscreenMenubarState::HIDDEN) {
    base::AutoReset<BOOL> autoReset(&shouldAnimateToolbarOut_, YES);
    [self ensureOverlayHiddenWithAnimation:YES];
  }

  [self removeTrackingAreaIfNecessary];
}

- (void)updateToolbar {
  [browserController_ layoutSubviews];
  [self updateTrackingArea];

  // In AppKit fullscreen, moving the mouse to the top of the screen toggles
  // menu visibility. Replicate the same effect for immersive fullscreen.
  if ([browserController_ isInImmersiveFullscreen])
    [self updateMenuBarAndDockVisibility];
}

- (BrowserWindowController*)browserWindowController {
  return browserController_;
}

// This method works, but is fragile.
//
// It gets used during view layout, which sometimes needs to be done at the
// beginning of an animation. As such, this method needs to reflect the
// menubarOffset expected at the end of the animation. This information is not
// readily available. (The layout logic needs a refactor).
//
// For AppKit Fullscreen, the menubar always starts hidden, and
// menubarFraction_ always starts at 0, so the logic happens to work. For
// Immersive Fullscreen, this class controls the visibility of the menu bar, so
// the logic is correct and not fragile.
- (CGFloat)menubarOffset {
  if ([browserController_ isInAppKitFullscreen]) {
    return -std::floor([menubarTracker_ menubarFraction] *
                       kToolbarVerticalOffset);
  }

  return [self shouldShowMenubarInImmersiveFullscreen] ? -kToolbarVerticalOffset
                                                       : 0;
}

- (CGFloat)toolbarFraction {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode))
    return kHideFraction;

  switch (slidingStyle_) {
    case FullscreenSlidingStyle::OMNIBOX_TABS_PRESENT:
      return kShowFraction;
    case FullscreenSlidingStyle::OMNIBOX_TABS_NONE:
      return kHideFraction;
    case FullscreenSlidingStyle::OMNIBOX_TABS_HIDDEN:
      FullscreenMenubarState menubarState = [menubarTracker_ state];
      if (menubarState == FullscreenMenubarState::SHOWN)
        return kShowFraction;

      if ([self mouseInsideTrackingArea])
        return kShowFraction;

      if (currentAnimation_.get())
        return [currentAnimation_ toolbarFraction];

      if (isLockingBarVisibility_)
        return kHideFraction;
      else if (isReleasingBarVisibility_)
        return kShowFraction;
      else if ([browserController_ isBarVisibilityLockedForOwner:nil])
        return kShowFraction;

      if (hideTimer_.get() || shouldAnimateToolbarOut_)
        return kShowFraction;

      return [menubarTracker_ menubarFraction];
  }
}

- (BOOL)isFullscreenTransitionInProgress {
  return [browserController_ isFullscreenTransitionInProgress];
}

- (BOOL)isInFullscreen {
  return inFullscreenMode_;
}

- (BOOL)isMouseOnScreen {
  return NSMouseInRect([NSEvent mouseLocation],
                       [[browserController_ window] screen].frame, false);
}

- (void)setTrackingAreaFromOverlayFrame:(NSRect)frame {
  NSRect contentBounds = [contentView_ bounds];
  trackingAreaFrame_ = frame;
  trackingAreaFrame_.origin.y -= kTrackingAreaAdditionalThreshold;
  trackingAreaFrame_.size.height =
      NSMaxY(contentBounds) - trackingAreaFrame_.origin.y;
}

- (void)animationDidStop:(NSAnimation*)animation {
  if (isRevealingToolbarForTabStripChanges_) {
    if ([self toolbarFraction] > 0.0) {
      // Set the timer to hide the toolbar.
      [hideTimer_ invalidate];
      hideTimer_.reset(
          [[NSTimer scheduledTimerWithTimeInterval:kTabStripChangesDelay
                                            target:self
                                          selector:@selector(hideTimerFire:)
                                          userInfo:nil
                                           repeats:NO] retain]);
    } else {
      isRevealingToolbarForTabStripChanges_ = NO;
    }
  }

  // Reset the |currentAnimation_| pointer now that the animation is over.
  currentAnimation_.reset();
}

- (void)animationDidEnd:(NSAnimation*)animation {
  [self animationDidStop:animation];
  [self updateTrackingArea];
}

@end

@implementation FullscreenToolbarController (PrivateMethods)

- (void)updateMenuBarAndDockVisibility {
  if (![self isMouseOnScreen] ||
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

- (void)updateTrackingArea {
  // Remove the tracking area if the toolbar isn't fully shown.
  if (!ui::IsCGFloatEqual([self toolbarFraction], kShowFraction)) {
    [self removeTrackingAreaIfNecessary];
    return;
  }

  if (trackingArea_) {
    // If the tracking rectangle is already |trackingAreaBounds_|, quit early.
    NSRect oldRect = [trackingArea_ rect];
    if (NSEqualRects(trackingAreaFrame_, oldRect))
      return;

    // Otherwise, remove it.
    [self removeTrackingAreaIfNecessary];
  }

  // Create and add a new tracking area for |frame|.
  trackingArea_.reset([[CrTrackingArea alloc]
      initWithRect:trackingAreaFrame_
           options:NSTrackingMouseEnteredAndExited | NSTrackingActiveInKeyWindow
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

- (BOOL)mouseInsideTrackingArea {
  if (!trackingArea_)
    return NO;

  NSWindow* window = [browserController_ window];
  NSPoint mouseLoc = [window mouseLocationOutsideOfEventStream];
  NSPoint mousePos = [contentView_ convertPoint:mouseLoc fromView:nil];
  return NSMouseInRect(mousePos, trackingAreaFrame_, [contentView_ isFlipped]);
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
  NSScreen* primaryScreen = [[NSScreen screens] firstObject];
  return (screen == primaryScreen);
}

- (base::mac::FullScreenMode)desiredSystemFullscreenMode {
  if ([self shouldShowMenubarInImmersiveFullscreen])
    return base::mac::kFullScreenModeHideDock;
  return base::mac::kFullScreenModeHideAll;
}

- (void)animateToolbarVisibility:(BOOL)visible {
  CGFloat fraction = visible ? kShowFraction : kHideFraction;

  // If we're already animating to the given fraction, then there's nothing
  // more to do.
  if (currentAnimation_ && [currentAnimation_ endFraction] == fraction)
    return;

  // In all other cases, we want to cancel any running animation (which may be
  // to show or to hide).
  [currentAnimation_ stopAnimation];

  // Create the animation and set it up.
  currentAnimation_.reset([[DropdownAnimation alloc]
      initWithFraction:fraction
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

- (void)cancelHideTimer {
  [hideTimer_ invalidate];
  hideTimer_.reset();
}

- (void)hideTimerFire:(NSTimer*)timer {
  DCHECK_EQ(hideTimer_, timer);  // This better be our hide timer.
  [hideTimer_ invalidate];       // Make sure it doesn't repeat.
  hideTimer_.reset();            // And get rid of it.
  base::AutoReset<BOOL> autoReset(&shouldAnimateToolbarOut_, YES);
  [self animateToolbarVisibility:NO];
}

- (void)cleanup {
  [self cancelAnimationAndTimer];
  [[NSNotificationCenter defaultCenter] removeObserver:self];

  [self removeTrackingAreaIfNecessary];

  // Call the main status resignation code to perform the associated cleanup,
  // since we will no longer be receiving actual status resignation
  // notifications.
  [self setSystemFullscreenModeTo:base::mac::kFullScreenModeNormal];

  menubarTracker_.reset();

  // No more calls back up to the BWC.
  browserController_ = nil;
}

- (BOOL)shouldShowMenubarInImmersiveFullscreen {
  return [self doesScreenHaveMenuBar] && [self toolbarFraction] > 0.99;
}

@end
