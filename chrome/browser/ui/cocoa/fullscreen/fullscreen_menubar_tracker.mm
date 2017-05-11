// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_menubar_tracker.h"

#include <Carbon/Carbon.h>

#include "base/mac/mac_util.h"
#include "base/macros.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_controller.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_visibility_lock_controller.h"
#include "ui/base/cocoa/appkit_utils.h"

namespace {

// The event kind value for a undocumented menubar show/hide Carbon event.
const CGFloat kMenuBarRevealEventKind = 2004;

OSStatus MenuBarRevealHandler(EventHandlerCallRef handler,
                              EventRef event,
                              void* context) {
  FullscreenMenubarTracker* self =
      static_cast<FullscreenMenubarTracker*>(context);

  // If Chrome has multiple fullscreen windows in their own space, the Handler
  // becomes flaky and might start receiving kMenuBarRevealEventKind events
  // from another space. Since the menubar in the another space is in either a
  // shown or hidden state, it will give us a reveal fraction of 0.0 or 1.0.
  // As such, we should ignore the kMenuBarRevealEventKind event if it gives
  // us a fraction of 0.0 or 1.0, and rely on kEventMenuBarShown and
  // kEventMenuBarHidden to set these values.
  if (GetEventKind(event) == kMenuBarRevealEventKind) {
    CGFloat revealFraction = 0;
    GetEventParameter(event, FOUR_CHAR_CODE('rvlf'), typeCGFloat, NULL,
                      sizeof(CGFloat), NULL, &revealFraction);
    if (revealFraction > 0.0 && revealFraction < 1.0)
      [self setMenubarProgress:revealFraction];
  } else if (GetEventKind(event) == kEventMenuBarShown) {
    [self setMenubarProgress:1.0];
  } else {
    [self setMenubarProgress:0.0];
  }

  return CallNextEventHandler(handler, event);
}

}  // end namespace

@interface FullscreenMenubarTracker () {
  // Our owner.
  FullscreenToolbarController* owner_;  // weak

  // The window's controller.
  BrowserWindowController* browserWindowController_;  // weak

  // A Carbon event handler that tracks the revealed fraction of the menubar.
  EventHandlerRef menubarTrackingHandler_;
}

// Returns YES if the mouse is on the same screen as the window.
- (BOOL)isMouseOnScreen;

@end

@implementation FullscreenMenubarTracker

@synthesize state = state_;
@synthesize menubarFraction = menubarFraction_;

- (instancetype)initWithFullscreenToolbarController:
    (FullscreenToolbarController*)owner {
  if ((self = [super init])) {
    owner_ = owner;
    browserWindowController_ = [owner_ browserWindowController];
    state_ = FullscreenMenubarState::HIDDEN;

    // Install the Carbon event handler for the menubar show, hide and
    // undocumented reveal event.
    EventTypeSpec eventSpecs[3];

    eventSpecs[0].eventClass = kEventClassMenu;
    eventSpecs[0].eventKind = kMenuBarRevealEventKind;

    eventSpecs[1].eventClass = kEventClassMenu;
    eventSpecs[1].eventKind = kEventMenuBarShown;

    eventSpecs[2].eventClass = kEventClassMenu;
    eventSpecs[2].eventKind = kEventMenuBarHidden;

    InstallApplicationEventHandler(NewEventHandlerUPP(&MenuBarRevealHandler),
                                   arraysize(eventSpecs), eventSpecs, self,
                                   &menubarTrackingHandler_);

    // Register for Active Space change notifications.
    [[[NSWorkspace sharedWorkspace] notificationCenter]
        addObserver:self
           selector:@selector(activeSpaceDidChange:)
               name:NSWorkspaceActiveSpaceDidChangeNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  RemoveEventHandler(menubarTrackingHandler_);
  [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self];

  [super dealloc];
}

- (CGFloat)menubarFraction {
  return menubarFraction_;
}

- (void)setMenubarProgress:(CGFloat)progress {
  if (![browserWindowController_ isInAnyFullscreenMode] ||
      [browserWindowController_ isFullscreenTransitionInProgress]) {
    return;
  }

  // If the menubarFraction increases, check if we are in the right screen
  // so that the toolbar is not revealed on the wrong screen.
  if (![self isMouseOnScreen] && progress > menubarFraction_)
    return;

  // Ignore the menubarFraction changes if the Space is inactive.
  if (![[browserWindowController_ window] isOnActiveSpace])
    return;

  if (ui::IsCGFloatEqual(progress, 1.0))
    state_ = FullscreenMenubarState::SHOWN;
  else if (ui::IsCGFloatEqual(progress, 0.0))
    state_ = FullscreenMenubarState::HIDDEN;
  else if (progress < menubarFraction_)
    state_ = FullscreenMenubarState::HIDING;
  else if (progress > menubarFraction_)
    state_ = FullscreenMenubarState::SHOWING;

  menubarFraction_ = progress;
  [owner_ updateToolbarLayout];

  // In 10.12. the toolbar to be janky since the UI doesn't update until the
  // menubar finished revealing itself. To smooth things out, animate the
  // toolbar in/out by locking/releasing its visibility instead of relying on
  // the menubar fraction.
  // TODO(spqchan): Figure out why it's not updating and make the toolbar drop
  // down in sync with the menubar. See crbug.com/672254.
  if (base::mac::IsOS10_12()) {
    if (state_ == FullscreenMenubarState::SHOWING) {
      [[owner_ visibilityLockController] lockToolbarVisibilityForOwner:self
                                                         withAnimation:YES];
    } else if (state_ == FullscreenMenubarState::HIDING) {
      [[owner_ visibilityLockController] releaseToolbarVisibilityForOwner:self
                                                            withAnimation:YES];
    }
  }
}

- (BOOL)isMouseOnScreen {
  return NSMouseInRect([NSEvent mouseLocation],
                       [[browserWindowController_ window] screen].frame, false);
}

- (void)activeSpaceDidChange:(NSNotification*)notification {
  menubarFraction_ = 0.0;
  state_ = FullscreenMenubarState::HIDDEN;
  [[owner_ visibilityLockController] releaseToolbarVisibilityForOwner:self
                                                        withAnimation:NO];
  [owner_ updateToolbarLayout];
}

@end
