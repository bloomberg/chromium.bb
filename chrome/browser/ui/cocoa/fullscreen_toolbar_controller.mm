// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/fullscreen_toolbar_controller.h"

#include <algorithm>

#include "base/command_line.h"
#import "base/mac/mac_util.h"
#include "base/mac/sdk_forward_declarations.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/common/chrome_switches.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMNSAnimation+Duration.h"
#import "ui/base/cocoa/nsview_additions.h"

namespace {

// The activation zone for the main menu is 4 pixels high; if we make it any
// smaller, then the menu can be made to appear without the bar sliding down.
const NSTimeInterval kDropdownAnimationDuration = 0.12;

// The duration the toolbar is revealed for tab strip changes.
const NSTimeInterval kDropdownForTabStripChangesDuration = 0.75;

// The event kind value for a undocumented menubar show/hide Carbon event.
const CGFloat kMenuBarRevealEventKind = 2004;

// The amount by which the floating bar is offset downwards (to avoid the menu)
// when the toolbar is hidden. (We can't use |-[NSMenu menuBarHeight]| since it
// returns 0 when the menu bar is hidden.)
const CGFloat kFloatingBarVerticalOffset = 22;

OSStatus MenuBarRevealHandler(EventHandlerCallRef handler,
                              EventRef event,
                              void* context) {
  FullscreenToolbarController* self =
      static_cast<FullscreenToolbarController*>(context);

  // If Chrome has multiple fullscreen windows in their own space, the Handler
  // becomes flaky and might start receiving kMenuBarRevealEventKind events
  // from another space. Since the menubar in the another space is in either a
  // shown or hidden state, it will give us a reveal fraction of 0.0 or 1.0.
  // As such, we should ignore the kMenuBarRevealEventKind event if it gives
  // us a fraction of 0.0 or 1.0, and rely on kEventMenuBarShown and
  // kEventMenuBarHidden to set these values.
  if (![self isFullscreenTransitionInProgress]) {
    if (GetEventKind(event) == kMenuBarRevealEventKind) {
      CGFloat revealFraction = 0;
      GetEventParameter(event, FOUR_CHAR_CODE('rvlf'), typeCGFloat, NULL,
                        sizeof(CGFloat), NULL, &revealFraction);
      if (revealFraction > 0.0 && revealFraction < 1.0)
        [self setMenuBarRevealProgress:revealFraction];
    } else if (GetEventKind(event) == kEventMenuBarShown) {
      [self setMenuBarRevealProgress:1.0];
    } else {
      [self setMenuBarRevealProgress:0.0];
    }
  }

  return CallNextEventHandler(handler, event);
}

}  // end namespace

// Helper class to manage animations for the dropdown bar.  Calls
// [FullscreenToolbarController changeToolbarFraction] once per
// animation step.
@interface DropdownAnimation : NSAnimation {
 @private
  FullscreenToolbarController* controller_;
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
            controller:(FullscreenToolbarController*)controller;

@end

@implementation DropdownAnimation

@synthesize startFraction = startFraction_;
@synthesize endFraction = endFraction_;

- (id)initWithFraction:(CGFloat)toFraction
          fullDuration:(CGFloat)fullDuration
        animationCurve:(NSAnimationCurve)animationCurve
            controller:(FullscreenToolbarController*)controller {
  // Calculate the effective duration, based on the current shown fraction.
  DCHECK(controller);
  CGFloat fromFraction = controller.toolbarFraction;
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
  [controller_ changeToolbarFraction:fraction];
}

@end

@interface FullscreenToolbarController (PrivateMethods)

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
- (void)changeOverlayToFraction:(CGFloat)fraction withAnimation:(BOOL)animate;

// Cancels the timer for hiding the floating bar.
- (void)cancelHideTimer;

// Methods called when the hide timers fire. Do not call directly.
- (void)hideTimerFire:(NSTimer*)timer;

// Stops any running animations, etc.
- (void)cleanup;

// Shows and hides the UI associated with this window being active (having main
// status).  This includes hiding the menu bar.  These functions are called when
// the window gains or loses main status as well as in |-cleanup|.
- (void)showActiveWindowUI;
- (void)hideActiveWindowUI;

// Whether the menu bar should be shown in immersive fullscreen for the screen
// that contains the window.
- (BOOL)shouldShowMenubarInImmersiveFullscreen;

@end

@implementation FullscreenToolbarController

@synthesize slidingStyle = slidingStyle_;
@synthesize toolbarFraction = toolbarFraction_;

- (id)initWithBrowserController:(BrowserWindowController*)controller
                          style:(fullscreen_mac::SlidingStyle)style {
  if ((self = [super init])) {
    browserController_ = controller;
    systemFullscreenMode_ = base::mac::kFullScreenModeNormal;
    slidingStyle_ = style;
  }

  // Install the Carbon event handler for the menubar show, hide and
  // undocumented reveal event.
  EventTypeSpec eventSpecs[3];

  eventSpecs[0].eventClass = kEventClassMenu;
  eventSpecs[0].eventKind = kMenuBarRevealEventKind;

  eventSpecs[1].eventClass = kEventClassMenu;
  eventSpecs[1].eventKind = kEventMenuBarShown;

  eventSpecs[2].eventClass = kEventClassMenu;
  eventSpecs[2].eventKind = kEventMenuBarHidden;

  InstallApplicationEventHandler(NewEventHandlerUPP(&MenuBarRevealHandler), 3,
                                 eventSpecs, self, &menuBarTrackingHandler_);

  return self;
}

- (void)dealloc {
  RemoveEventHandler(menuBarTrackingHandler_);
  DCHECK(!inFullscreenMode_);
  [super dealloc];
}

- (void)setupFullscreenToolbarWithDropdown:(BOOL)showDropdown {
  DCHECK(!inFullscreenMode_);
  inFullscreenMode_ = YES;
  [self changeToolbarFraction:(showDropdown ? 1 : 0)];
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
  [self showActiveWindowUI];
}

- (void)windowDidResignMain:(NSNotification*)notification {
  [self hideActiveWindowUI];
}

- (CGFloat)floatingBarVerticalOffset {
  return kFloatingBarVerticalOffset;
}

- (void)ensureOverlayShownWithAnimation:(BOOL)animate {
  if (!inFullscreenMode_)
    return;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode))
    return;

  if (self.slidingStyle == fullscreen_mac::OMNIBOX_TABS_PRESENT)
    return;

  [self cancelHideTimer];
  [self changeOverlayToFraction:1 withAnimation:animate];
}

- (void)ensureOverlayHiddenWithAnimation:(BOOL)animate {
  if (!inFullscreenMode_)
    return;

  if (self.slidingStyle == fullscreen_mac::OMNIBOX_TABS_PRESENT)
    return;

  [self cancelHideTimer];
  [self changeOverlayToFraction:0 withAnimation:animate];
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

  revealToolbarForTabStripChanges_ = YES;
  [self ensureOverlayShownWithAnimation:YES];
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

- (void)changeToolbarFraction:(CGFloat)fraction {
  toolbarFraction_ = fraction;
  [browserController_ layoutSubviews];

  // In AppKit fullscreen, moving the mouse to the top of the screen toggles
  // menu visibility. Replicate the same effect for immersive fullscreen.
  if ([browserController_ isInImmersiveFullscreen])
    [self updateMenuBarAndDockVisibility];
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
  if ([browserController_ isInAppKitFullscreen])
    return -std::floor(menubarFraction_ * [self floatingBarVerticalOffset]);

  return [self shouldShowMenubarInImmersiveFullscreen]
             ? -[self floatingBarVerticalOffset]
             : 0;
}

- (BOOL)isFullscreenTransitionInProgress {
  return [browserController_ isFullscreenTransitionInProgress];
}

- (BOOL)isMouseOnScreen {
  return NSMouseInRect([NSEvent mouseLocation],
                       [[browserController_ window] screen].frame, false);
}

- (void)animationDidStop:(NSAnimation*)animation {
  // Reset the |currentAnimation_| pointer now that the animation is over.
  currentAnimation_.reset();

  if (revealToolbarForTabStripChanges_) {
    if (toolbarFraction_ > 0.0) {
      // Set the timer to hide the toolbar.
      [hideTimer_ invalidate];
      hideTimer_.reset([[NSTimer
          scheduledTimerWithTimeInterval:kDropdownForTabStripChangesDuration
                                  target:self
                                selector:@selector(hideTimerFire:)
                                userInfo:nil
                                 repeats:NO] retain]);
    } else {
      revealToolbarForTabStripChanges_ = NO;
    }
  }
}

- (void)animationDidEnd:(NSAnimation*)animation {
  [self animationDidStop:animation];
}

- (void)setMenuBarRevealProgress:(CGFloat)progress {
  // If the menubarFraction increases, check if we are in the right screen
  // so that the toolbar is not revealed on the wrong screen.
  if (![self isMouseOnScreen] && progress > menubarFraction_)
    return;

  menubarFraction_ = progress;

  // If an animation is not running, then -layoutSubviews will not be called
  // for each tick of the menu bar reveal. Do that manually.
  // TODO(erikchen): The animation is janky. layoutSubviews need a refactor so
  // that it calls setFrameOffset: instead of setFrame: if the frame's size has
  // not changed.
  if (!currentAnimation_.get()) {
    if (self.slidingStyle != fullscreen_mac::OMNIBOX_TABS_NONE)
      toolbarFraction_ = progress;
    [browserController_ layoutSubviews];
  }
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

- (void)changeOverlayToFraction:(CGFloat)fraction withAnimation:(BOOL)animate {
  // The non-animated case is really simple, so do it and return.
  if (!animate) {
    [currentAnimation_ stopAnimation];
    [self changeToolbarFraction:fraction];
    return;
  }

  // If we're already animating to the given fraction, then there's nothing more
  // to do.
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
  [self changeOverlayToFraction:0 withAnimation:YES];
}

- (void)cleanup {
  [self cancelAnimationAndTimer];
  [[NSNotificationCenter defaultCenter] removeObserver:self];

  // This isn't tracked when not in fullscreen mode.
  [browserController_ releaseBarVisibilityForOwner:self withAnimation:NO];

  // Call the main status resignation code to perform the associated cleanup,
  // since we will no longer be receiving actual status resignation
  // notifications.
  [self setSystemFullscreenModeTo:base::mac::kFullScreenModeNormal];

  // No more calls back up to the BWC.
  browserController_ = nil;
}

- (void)showActiveWindowUI {
  [self updateMenuBarAndDockVisibility];
}

- (void)hideActiveWindowUI {
  [self updateMenuBarAndDockVisibility];
}

- (BOOL)shouldShowMenubarInImmersiveFullscreen {
  return [self doesScreenHaveMenuBar] && toolbarFraction_ > 0.99;
}

@end
