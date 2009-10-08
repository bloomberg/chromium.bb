// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_CHROME_BROWSER_WINDOW_H_
#define CHROME_BROWSER_COCOA_CHROME_BROWSER_WINDOW_H_

#import <Cocoa/Cocoa.h>

// Cocoa class representing a Chrome browser window.
// We need to override NSWindow with our own class since we need access to all
// unhandled keyboard events and subclassing NSWindow is the only method to do
// this.
@interface ChromeBrowserWindow : NSWindow {
  BOOL shouldHideTitle_;
}

// Override, so we can handle global keyboard events.
- (BOOL)performKeyEquivalent:(NSEvent*)theEvent;

// Tells the window to suppress title drawing.
- (void)setShouldHideTitle:(BOOL)flag;

@end

@interface ChromeBrowserWindow (UndocumentedAPI)

// Undocumented Cocoa API to suppress drawing of the window's title.
// -setTitle: still works, but the title set only applies to the
// miniwindow and menus (and, importantly, Expose).  Overridden to
// return |shouldHideTitle_|.
-(BOOL)_isTitleHidden;

@end

#endif  // CHROME_BROWSER_COCOA_CHROME_BROWSER_WINDOW_H_
