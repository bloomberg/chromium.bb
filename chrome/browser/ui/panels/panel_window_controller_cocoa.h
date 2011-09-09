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
#import "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/browser_command_executor.h"
#import "chrome/browser/ui/cocoa/themed_window.h"

@class FindBarCocoaController;
class PanelBrowserWindowCocoa;
@class PanelTitlebarViewCocoa;

@interface PanelWindowControllerCocoa : NSWindowController<NSWindowDelegate,
                                                       BrowserCommandExecutor> {
 @private
  IBOutlet PanelTitlebarViewCocoa* titlebar_view_;
  scoped_ptr<PanelBrowserWindowCocoa> windowShim_;
  scoped_nsobject<NSString> pendingWindowTitle_;
  NSViewAnimation* boundsAnimation_;  // Lifetime controlled manually, needs
                                      // more then just |release| to terminate.
  BOOL animateOnBoundsChange_;
}

// Load the browser window nib and do any Cocoa-specific initialization.
- (id)initWithBrowserWindow:(PanelBrowserWindowCocoa*)window;

- (ui::ThemeProvider*)themeProvider;
- (ThemedWindowStyle)themedWindowStyle;
- (NSPoint)themePatternPhase;

// Returns the TabContents' native view. It renders the content of the web page
// in the Panel.
- (NSView*)tabContentsView;

// Sometimes (when we animate the size of the window) we want to stop resizing
// the TabContents's cocoa view to avoid unnecessary churn and issues
// that can be caused by sizes near 0.
- (void)disableTabContentsViewAutosizing;
- (void)enableTabContentsViewAutosizing;

// Shows the window for the first time. Only happens once.
- (void)revealAnimatedWithFrame:(const NSRect&)frame;

- (void)updateTitleBar;

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
- (void)setPanelFrame:(NSRect)frame;

// Used by PanelTitlebarViewCocoa when user rearranges the Panels by dragging.
- (void)startDrag;
- (void)endDrag:(BOOL)cancelled;
- (void)dragWithDeltaX:(int)deltaX;

// Accessor for titlebar view.
- (PanelTitlebarViewCocoa*)titlebarView;

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

@end  // @interface PanelWindowController

// Methods which are either only for testing, or only public for testing.
@interface PanelWindowControllerCocoa(TestingAPI)

+ (void)enableBoundsAnimationNotifications;

@end  // @interface PanelWindowControllerCocoa(TestingAPI)

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_WINDOW_CONTROLLER_COCOA_H_
