// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_WINDOW_CONTROLLER_COCOA_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_WINDOW_CONTROLLER_COCOA_H_

// A class acting as the Objective-C controller for the Panel window
// object. Handles interactions between Cocoa and the cross-platform
// code. Each window has a single titlebar and is managed/owned by Panel.

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#import "chrome/browser/ui/cocoa/chrome_browser_window.h"
#import "chrome/browser/ui/cocoa/tab_contents/tab_contents_controller.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "chrome/browser/ui/cocoa/tracking_area.h"
#include "chrome/browser/ui/panels/panel.h"

class PanelCocoa;
@class PanelTitlebarViewCocoa;

@interface PanelWindowCocoaImpl : ChromeBrowserWindow {
}
@end

@interface PanelWindowControllerCocoa : NSWindowController
                                            <NSWindowDelegate,
                                             NSAnimationDelegate> {
 @private
  IBOutlet PanelTitlebarViewCocoa* titlebar_view_;
  scoped_ptr<PanelCocoa> windowShim_;
  scoped_nsobject<NSString> pendingWindowTitle_;
  scoped_nsobject<TabContentsController> contentsController_;
  NSViewAnimation* boundsAnimation_;  // Lifetime controlled manually, needs
                                      // more then just |release| to terminate.
  BOOL animateOnBoundsChange_;
  BOOL throbberShouldSpin_;
  BOOL playingMinimizeAnimation_;
  float animationStopToShowTitlebarOnly_;
  BOOL canBecomeKeyWindow_;
  // Allow a panel to become key if activated via Panel logic, as opposed
  // to by default system selection. The system will prefer a panel
  // window over other application windows due to panels having a higher
  // priority NSWindowLevel, so we distinguish between the two scenarios.
  BOOL activationRequestedByPanel_;
  scoped_nsobject<NSView> overlayView_;
}

// Load the window nib and do any Cocoa-specific initialization.
- (id)initWithPanel:(PanelCocoa*)window;

- (ui::ThemeProvider*)themeProvider;
- (ThemedWindowStyle)themedWindowStyle;
- (NSPoint)themePatternPhase;

- (void)webContentsInserted:(content::WebContents*)contents;
- (void)webContentsDetached:(content::WebContents*)contents;

// Sometimes (when we animate the size of the window) we want to stop resizing
// the WebContents's Cocoa view to avoid unnecessary rendering and issues
// that can be caused by sizes near 0.
- (void)disableTabContentsViewAutosizing;
- (void)enableTabContentsViewAutosizing;

// Shows the window for the first time. Only happens once.
- (void)revealAnimatedWithFrame:(const NSRect&)frame;

- (void)updateTitleBar;
- (void)updateIcon;
- (void)updateThrobber:(BOOL)shouldSpin;
- (void)updateTitleBarMinimizeRestoreButtonVisibility;

// Initiate the closing of the panel, starting from the platform-independent
// layer. This will take care of PanelManager, other panels and close the
// native window at the end.
- (void)closePanel;

// Minimize/Restore the panel or all panels, depending on the modifier.
// Invoked when the minimize/restore button is clicked.
- (void)minimizeButtonClicked:(int)modifierFlags;
- (void)restoreButtonClicked:(int)modifierFlags;

// Uses nonblocking animation for moving the Panels. It's especially
// important in case of dragging a Panel when other Panels should 'slide out',
// indicating the potential drop slot.
// |frame| is in screen coordinates, same as [window frame].
// |animate| controls if the bounds animation is needed or not.
- (void)setPanelFrame:(NSRect)frame
              animate:(BOOL)animate;

// Used by PanelTitlebarViewCocoa when user rearranges the Panels by dragging.
// |mouseLocation| is in Cocoa's screen coordinates.
- (void)startDrag:(NSPoint)mouseLocation;
- (void)endDrag:(BOOL)cancelled;
- (void)drag:(NSPoint)mouseLocation;

// Accessor for titlebar view.
- (PanelTitlebarViewCocoa*)titlebarView;
// Returns the height of titlebar, used to show the titlebar in
// "Draw Attention" state.
- (int)titlebarHeightInScreenCoordinates;

// Invoked when user clicks on the titlebar. Attempts to flip the
// Minimized/Restored states.
- (void)onTitlebarMouseClicked:(int)modifierFlags;

// NSAnimationDelegate method, invoked when bounds animation is finished.
- (void)animationDidEnd:(NSAnimation*)animation;
// Terminates current bounds animation, if any.
- (void)terminateBoundsAnimation;

- (BOOL)isAnimatingBounds;

// Sets/Removes the Key status from the panel to some other window.
- (void)activate;
- (void)deactivate;

// Changes the canBecomeKeyWindow state
- (void)preventBecomingKeyWindow:(BOOL)prevent;

// See Panel::FullScreenModeChanged.
- (void)fullScreenModeChanged:(bool)isFullScreen;

// Helper for NSWindow, returns NO for minimized panels in some cases, so they
// are not un-minimized when another panel is minimized.
- (BOOL)canBecomeKeyWindow;

// Returns true if Panel requested activation of the window.
- (BOOL)activationRequestedByPanel;

- (void)ensureFullyVisible;

// Adjust NSStatusWindowLevel based on whether panel is always on top
// and whether the panel is minimized. The first version wraps the second
// version using the current panel expanstion state.
- (void)updateWindowLevel;
- (void)updateWindowLevel:(BOOL)panelIsMinimized;

// Turns on user-resizable corners/sides indications and enables live resize.
- (void)enableResizeByMouse:(BOOL)enable;

- (NSRect)frameRectForContentRect:(NSRect)contentRect;
- (NSRect)contentRectForFrameRect:(NSRect)frameRect;

@end  // @interface PanelWindowController

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_WINDOW_CONTROLLER_COCOA_H_
