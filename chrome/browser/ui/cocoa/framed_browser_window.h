// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_FRAMED_BROWSER_WINDOW_H_
#define CHROME_BROWSER_UI_COCOA_FRAMED_BROWSER_WINDOW_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/chrome_browser_window.h"

// Offsets from the top/left of the window frame to the top of the window
// controls (zoom, close, miniaturize) for a window with a tabstrip.
const NSInteger kFramedWindowButtonsWithTabStripOffsetFromTop = 11;
const NSInteger kFramedWindowButtonsWithTabStripOffsetFromLeft = 11;

// Offsets from the top/left of the window frame to the top of the window
// controls (zoom, close, miniaturize) for a window without a tabstrip.
const NSInteger kFramedWindowButtonsWithoutTabStripOffsetFromTop = 4;
const NSInteger kFramedWindowButtonsWithoutTabStripOffsetFromLeft = 8;

// Offset between the window controls (zoom, close, miniaturize).
const NSInteger kFramedWindowButtonsInterButtonSpacing = 7;

// Cocoa class representing a framed browser window.
// We need to override NSWindow with our own class since we need access to all
// unhandled keyboard events and subclassing NSWindow is the only method to do
// this. We also handle our own window controls and custom window frame drawing.
@interface FramedBrowserWindow : ChromeBrowserWindow {
 @private
  BOOL shouldHideTitle_;
  NSButton* closeButton_;
  NSButton* miniaturizeButton_;
  NSButton* zoomButton_;
  BOOL entered_;
  scoped_nsobject<NSTrackingArea> widgetTrackingArea_;
}

// Tells the window to suppress title drawing.
- (void)setShouldHideTitle:(BOOL)flag;

// Return true if the mouse is currently in our tracking area for our window
// widgets.
- (BOOL)mouseInGroup:(NSButton*)widget;

// Update the tracking areas for our window widgets as appropriate.
- (void)updateTrackingAreas;

@end

@interface NSWindow (UndocumentedAPI)

// Undocumented Cocoa API to suppress drawing of the window's title.
// -setTitle: still works, but the title set only applies to the
// miniwindow and menus (and, importantly, Expose).  Overridden to
// return |shouldHideTitle_|.
-(BOOL)_isTitleHidden;

@end

#endif  // CHROME_BROWSER_UI_COCOA_FRAMED_BROWSER_WINDOW_H_
