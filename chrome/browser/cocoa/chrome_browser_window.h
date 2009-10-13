// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_CHROME_BROWSER_WINDOW_H_
#define CHROME_BROWSER_COCOA_CHROME_BROWSER_WINDOW_H_

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"

// Offset from the top of the window frame to the top of the window controls
// (zoom, close, miniaturize) for a window with a tabstrip.
const NSInteger kChromeWindowButtonsWithTabStripOffsetFromTop = 6;

// Offset from the top of the window frame to the top of the window controls
// (zoom, close, miniaturize) for a window without a tabstrip.
const NSInteger kChromeWindowButtonsWithoutTabStripOffsetFromTop = 4;

// Offset from the left of the window frame to the top of the window controls
// (zoom, close, miniaturize).
const NSInteger kChromeWindowButtonsOffsetFromLeft = 8;

// Offset between the window controls (zoom, close, miniaturize).
const NSInteger kChromeWindowButtonsInterButtonSpacing = 7;

// Cocoa class representing a Chrome browser window.
// We need to override NSWindow with our own class since we need access to all
// unhandled keyboard events and subclassing NSWindow is the only method to do
// this. We also handle our own window controls and custom window frame drawing.
@interface ChromeBrowserWindow : NSWindow {
 @private
  BOOL shouldHideTitle_;
  NSButton* closeButton_;
  NSButton* miniaturizeButton_;
  NSButton* zoomButton_;
  BOOL entered_;
  scoped_nsobject<NSTrackingArea> widgetTrackingArea_;
}

// See global_keyboard_shortcuts_mac.h for details on the next two functions.

// Checks if |event| is a window keyboard shortcut. If so, dispatches it to the
// window controller's |executeCommand:| and returns |YES|.
- (BOOL)handleExtraWindowKeyboardShortcut:(NSEvent*)event;

// Checks if |event| is a browser keyboard shortcut. If so, dispatches it to the
// window controller's |executeCommand:| and returns |YES|.
- (BOOL)handleExtraBrowserKeyboardShortcut:(NSEvent*)event;

// Override, so we can handle global keyboard events.
- (BOOL)performKeyEquivalent:(NSEvent*)theEvent;

// Tells the window to suppress title drawing.
- (void)setShouldHideTitle:(BOOL)flag;

// Return true if the mouse is currently in our tracking area for our window
// widgets.
- (BOOL)mouseInGroup:(NSButton*)widget;

// Update the tracking areas for our window widgets as appropriate.
- (void)updateTrackingAreas;
@end

@interface ChromeBrowserWindow (UndocumentedAPI)

// Undocumented Cocoa API to suppress drawing of the window's title.
// -setTitle: still works, but the title set only applies to the
// miniwindow and menus (and, importantly, Expose).  Overridden to
// return |shouldHideTitle_|.
-(BOOL)_isTitleHidden;

@end

#endif  // CHROME_BROWSER_COCOA_CHROME_BROWSER_WINDOW_H_
