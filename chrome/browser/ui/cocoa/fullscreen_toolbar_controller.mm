// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/fullscreen_toolbar_controller.h"

#include <algorithm>

#include "base/command_line.h"
#import "base/mac/mac_util.h"
#include "base/mac/sdk_forward_declarations.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_menubar_tracker.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_animation_controller.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_mouse_tracker.h"
#include "chrome/common/chrome_switches.h"
#include "ui/base/cocoa/appkit_utils.h"
#import "ui/base/cocoa/nsview_additions.h"
#import "ui/base/cocoa/tracking_area.h"

namespace {

// Visibility fractions for the menubar and toolbar.
const CGFloat kHideFraction = 0.0;
const CGFloat kShowFraction = 1.0;

// The amount by which the toolbar is offset downwards (to avoid the menu)
// when the toolbar style is OMNIBOX_TABS_HIDDEN. (We can't use
// |-[NSMenu menuBarHeight]| since it returns 0 when the menu bar is hidden.)
const CGFloat kToolbarVerticalOffset = 22;

}  // end namespace

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
    animationController_.reset(new FullscreenToolbarAnimationController(self));
  }

  return self;
}

- (void)dealloc {
  DCHECK(!inFullscreenMode_);
  [super dealloc];
}

- (void)enterFullscreenMode {
  DCHECK(!inFullscreenMode_);
  inFullscreenMode_ = YES;

  menubarTracker_.reset([[FullscreenMenubarTracker alloc]
      initWithFullscreenToolbarController:self]);
  mouseTracker_.reset([[FullscreenToolbarMouseTracker alloc]
      initWithFullscreenToolbarController:self
                      animationController:animationController_.get()]);

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

- (void)ensureOverlayShownWithAnimation:(BOOL)animate {
  animationController_->AnimateToolbarIn();
}

- (void)ensureOverlayHiddenWithAnimation:(BOOL)animate {
  animationController_->AnimateToolbarOutIfPossible();
}

// Cancels any running animation and timers.
- (void)cancelAnimationAndTimer {
  animationController_->StopAnimationAndTimer();
}

- (void)revealToolbarForTabStripChanges {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableFullscreenToolbarReveal)) {
    return;
  }

  animationController_->AnimateToolbarForTabstripChanges();
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

- (void)updateToolbar {
  [browserController_ layoutSubviews];
  animationController_->ToolbarDidUpdate();
  [mouseTracker_ updateTrackingArea];

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
      if ([self mustShowFullscreenToolbar])
        return kShowFraction;

      if (animationController_->IsAnimationRunning())
        return animationController_->GetToolbarFractionFromProgress();

      return [menubarTracker_ menubarFraction];
  }
}

- (BOOL)mustShowFullscreenToolbar {
  if (!inFullscreenMode_)
    return NO;

  if (slidingStyle_ == FullscreenSlidingStyle::OMNIBOX_TABS_PRESENT)
    return YES;

  if (slidingStyle_ == FullscreenSlidingStyle::OMNIBOX_TABS_NONE)
    return NO;

  FullscreenMenubarState menubarState = [menubarTracker_ state];
  return menubarState == FullscreenMenubarState::SHOWN ||
         [mouseTracker_ mouseInsideTrackingArea] ||
         [browserController_ isBarVisibilityLockedForOwner:nil];
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

- (void)updateToolbarFrame:(NSRect)frame {
  if (mouseTracker_.get())
    [mouseTracker_ updateToolbarFrame:frame];
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

- (void)cleanup {
  animationController_->StopAnimationAndTimer();
  [[NSNotificationCenter defaultCenter] removeObserver:self];

  // Call the main status resignation code to perform the associated cleanup,
  // since we will no longer be receiving actual status resignation
  // notifications.
  [self setSystemFullscreenModeTo:base::mac::kFullScreenModeNormal];

  menubarTracker_.reset();
  mouseTracker_.reset();

  // No more calls back up to the BWC.
  browserController_ = nil;
}

- (BOOL)shouldShowMenubarInImmersiveFullscreen {
  return [self doesScreenHaveMenuBar] && [self toolbarFraction] > 0.99;
}

@end
