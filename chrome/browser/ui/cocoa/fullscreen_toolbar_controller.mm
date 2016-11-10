// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/fullscreen_toolbar_controller.h"

#include <algorithm>

#include "base/command_line.h"
#import "base/mac/mac_util.h"
#include "base/mac/sdk_forward_declarations.h"
#include "chrome/browser/profiles/profile.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_menubar_tracker.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_animation_controller.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_mouse_tracker.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_visibility_lock_controller.h"
#import "chrome/browser/ui/cocoa/fullscreen/immersive_fullscreen_controller.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "ui/base/cocoa/appkit_utils.h"
#import "ui/base/cocoa/nsview_additions.h"

namespace {

// Visibility fractions for the menubar and toolbar.
const CGFloat kHideFraction = 0.0;
const CGFloat kShowFraction = 1.0;

// The amount by which the toolbar is offset downwards (to avoid the menu)
// when the toolbar style is TOOLBAR_HIDDEN. (We can't use
// |-[NSMenu menuBarHeight]| since it returns 0 when the menu bar is hidden.)
const CGFloat kToolbarVerticalOffset = 22;

}  // end namespace

@implementation FullscreenToolbarController

@synthesize toolbarStyle = toolbarStyle_;

- (id)initWithBrowserController:(BrowserWindowController*)controller {
  if ((self = [super init])) {
    browserController_ = controller;
    animationController_.reset(new FullscreenToolbarAnimationController(self));
    visibilityLockController_.reset(
        [[FullscreenToolbarVisibilityLockController alloc]
            initWithFullscreenToolbarController:self
                            animationController:animationController_.get()]);
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

  [self updateToolbarStyle];

  if ([browserController_ isInImmersiveFullscreen]) {
    immersiveFullscreenController_.reset([[ImmersiveFullscreenController alloc]
        initWithBrowserController:browserController_]);
    [immersiveFullscreenController_ updateMenuBarAndDockVisibility];
  } else {
    menubarTracker_.reset([[FullscreenMenubarTracker alloc]
        initWithFullscreenToolbarController:self]);
    mouseTracker_.reset([[FullscreenToolbarMouseTracker alloc]
        initWithFullscreenToolbarController:self
                        animationController:animationController_.get()]);
  }
}

- (void)exitFullscreenMode {
  DCHECK(inFullscreenMode_);
  inFullscreenMode_ = NO;

  animationController_->StopAnimationAndTimer();
  [[NSNotificationCenter defaultCenter] removeObserver:self];

  menubarTracker_.reset();
  mouseTracker_.reset();
  immersiveFullscreenController_.reset();

  // No more calls back up to the BWC.
  browserController_ = nil;
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

- (void)updateToolbarStyle {
  FullscreenToolbarStyle oldStyle = toolbarStyle_;

  if ([browserController_ isFullscreenForTabContentOrExtension]) {
    toolbarStyle_ = FullscreenToolbarStyle::TOOLBAR_NONE;
  } else {
    PrefService* prefs = [browserController_ profile]->GetPrefs();
    toolbarStyle_ = prefs->GetBoolean(prefs::kShowFullscreenToolbar)
                        ? FullscreenToolbarStyle::TOOLBAR_PRESENT
                        : FullscreenToolbarStyle::TOOLBAR_HIDDEN;
  }

  if (oldStyle != toolbarStyle_)
    [self updateToolbar];
}

- (void)updateToolbar {
  [browserController_ layoutSubviews];
  animationController_->ToolbarDidUpdate();
  [mouseTracker_ updateTrackingArea];
}

- (BrowserWindowController*)browserWindowController {
  return browserController_;
}

- (FullscreenToolbarVisibilityLockController*)visibilityLockController {
  return visibilityLockController_.get();
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

  return [immersiveFullscreenController_ shouldShowMenubar]
             ? -kToolbarVerticalOffset
             : 0;
}

- (CGFloat)toolbarFraction {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode))
    return kHideFraction;

  switch (toolbarStyle_) {
    case FullscreenToolbarStyle::TOOLBAR_PRESENT:
      return kShowFraction;
    case FullscreenToolbarStyle::TOOLBAR_NONE:
      return kHideFraction;
    case FullscreenToolbarStyle::TOOLBAR_HIDDEN:
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

  if (toolbarStyle_ == FullscreenToolbarStyle::TOOLBAR_PRESENT)
    return YES;

  if (toolbarStyle_ == FullscreenToolbarStyle::TOOLBAR_NONE)
    return NO;

  FullscreenMenubarState menubarState = [menubarTracker_ state];
  return menubarState == FullscreenMenubarState::SHOWN ||
         [mouseTracker_ mouseInsideTrackingArea] ||
         [visibilityLockController_ isToolbarVisibilityLocked];
}

- (BOOL)isInFullscreen {
  return inFullscreenMode_;
}

- (void)updateToolbarFrame:(NSRect)frame {
  if (mouseTracker_.get())
    [mouseTracker_ updateToolbarFrame:frame];
}

@end

