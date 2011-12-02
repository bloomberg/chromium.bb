// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_WINDOW_CONTROLLER_COCOA_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_WINDOW_CONTROLLER_COCOA_H_

// A class acting as the Objective-C controller for the Panel window
// object. Handles interactions between Cocoa and the cross-platform
// code. Each window has a single titlebar and is managed/owned by Panel.

#import <Cocoa/Cocoa.h>

#import "base/mac/cocoa_protocols.h"
#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#import "chrome/browser/ui/cocoa/browser_command_executor.h"
#import "chrome/browser/ui/cocoa/chrome_browser_window.h"
#import "chrome/browser/ui/cocoa/tab_contents/tab_contents_controller.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "chrome/browser/ui/cocoa/tracking_area.h"

@class FindBarCocoaController;
class PanelBrowserWindowCocoa;
@class PanelTitlebarViewCocoa;

@interface PanelWindowCocoaImpl : ChromeBrowserWindow {
}
// The panels cannot be reduced to 3-px windows on the edge of the screen
// active area (above Dock). Default constraining logic makes at least a height
// of the titlebar visible, so the user could still grab it. We do 'restore'
// differently, and minimize panels to 3 px. Hence the need to override the
// constraining logic.
- (NSRect)constrainFrameRect:(NSRect)frameRect toScreen:(NSScreen *)screen;

// Prevent panel window from becoming key - for example when it is minimized.
- (BOOL)canBecomeKeyWindow;
@end

@interface PanelWindowControllerCocoa : NSWindowController
                                            <NSWindowDelegate,
                                             NSAnimationDelegate,
                                             BrowserCommandExecutor> {
 @private
  IBOutlet PanelTitlebarViewCocoa* titlebar_view_;
  scoped_ptr<PanelBrowserWindowCocoa> windowShim_;
  scoped_nsobject<NSString> pendingWindowTitle_;
  scoped_nsobject<TabContentsController> contentsController_;
  NSViewAnimation* boundsAnimation_;  // Lifetime controlled manually, needs
                                      // more then just |release| to terminate.
  BOOL animateOnBoundsChange_;
  ScopedCrTrackingArea windowTrackingArea_;
  BOOL throbberShouldSpin_;
  BOOL playingMinimizeAnimation_;
  float animationStopToShowTitlebarOnly_;
}

// Load the browser window nib and do any Cocoa-specific initialization.
- (id)initWithBrowserWindow:(PanelBrowserWindowCocoa*)window;

- (ui::ThemeProvider*)themeProvider;
- (ThemedWindowStyle)themedWindowStyle;
- (NSPoint)themePatternPhase;

- (void)tabInserted:(TabContents*)contents;
- (void)tabDetached:(TabContents*)contents;

// Sometimes (when we animate the size of the window) we want to stop resizing
// the TabContents' cocoa view to avoid unnecessary rendering and issues
// that can be caused by sizes near 0.
- (void)disableTabContentsViewAutosizing;
- (void)enableTabContentsViewAutosizing;

// Shows the window for the first time. Only happens once.
- (void)revealAnimatedWithFrame:(const NSRect&)frame;

- (void)updateTitleBar;
- (void)updateIcon;
- (void)updateThrobber:(BOOL)shouldSpin;

// Adds the FindBar controller's view to this Panel. Must only be
// called once per PanelWindowControllerCocoa.
- (void)addFindBar:(FindBarCocoaController*)findBarCocoaController;

// Initiate the closing of the panel, starting from the platform-independent
// layer. This will take care of PanelManager, other panels and close the
// native window at the end.
- (void)closePanel;

// Uses nonblocking animation for moving the Panels. It's especially
// important in case of dragging a Panel when other Panels should 'slide out',
// indicating the potential drop slot.
// |frame| is in screen coordinates, same as [window frame].
// |animate| controls if the bounds animation is needed or not.
- (void)setPanelFrame:(NSRect)frame
              animate:(BOOL)animate;

// Used by PanelTitlebarViewCocoa when user rearranges the Panels by dragging.
- (void)startDrag;
- (void)endDrag:(BOOL)cancelled;
- (void)dragWithDeltaX:(int)deltaX;

// Accessor for titlebar view.
- (PanelTitlebarViewCocoa*)titlebarView;
// Returns the height of titlebar, used to show the titlebar in
// "Draw Attention" state.
- (int)titlebarHeightInScreenCoordinates;

// Invoked when user clicks on the titlebar. Attempts to flip the
// Minimized/Restored states.
- (void)onTitlebarMouseClicked;

// Executes the command in the context of the current browser.
// |command| is an integer value containing one of the constants defined in the
// "chrome/app/chrome_command_ids.h" file.
- (void)executeCommand:(int)command;

// Invokes the settings menu when the settings button is pressed.
- (void)runSettingsMenu:(NSView*)button;

// NSAnimationDelegate method, invoked when bounds animation is finished.
- (void)animationDidEnd:(NSAnimation*)animation;
// Terminates current bounds animation, if any.
- (void)terminateBoundsAnimation;

- (BOOL)isAnimatingBounds;

// Removes the Key status from the panel to some other window.
- (void)deactivate;

// Helper for NSWindow, returns NO for minimized panels in some cases, so they
// are not un-minimized when another panel is minimized.
- (BOOL)canBecomeKeyWindow;

@end  // @interface PanelWindowController

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_WINDOW_CONTROLLER_COCOA_H_
