// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_UTILS_H_
#define CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_UTILS_H_
#pragma once

#import <Cocoa/Cocoa.h>

class Browser;

namespace content {
struct NativeWebKeyboardEvent;
}

@interface BrowserWindowUtils : NSObject

// Returns YES if keyboard event should be handled.
+ (BOOL)shouldHandleKeyboardEvent:(const content::NativeWebKeyboardEvent&)event;

// Determines the command associated with the keyboard event.
// Returns -1 if no command found.
+ (int)getCommandId:(const content::NativeWebKeyboardEvent&)event;

// NSWindow must be a ChromeEventProcessingWindow.
+ (BOOL)handleKeyboardEvent:(NSEvent*)event
                   inWindow:(NSWindow*)window;

// Schedule a window title change in the next run loop iteration. This works
// around a Cocoa bug: if a window changes title during the tracking of the
// Window menu it doesn't display well and the constant re-sorting of the list
// makes it difficult for the user to pick the desired window.
// Passing in a non-nil oldTitle will also cancel any pending title changes with
// a matching window and title. This function returns a NSString* that can be
// passed in future calls as oldTitle.
+ (NSString*)scheduleReplaceOldTitle:(NSString*)oldTitle
                        withNewTitle:(NSString*)newTitle
                           forWindow:(NSWindow*)window;

+ (NSPoint)themePatternPhaseFor:(NSView*)windowView
                   withTabStrip:(NSView*)tabStripView;

+ (void)activateWindowForController:(NSWindowController*)controller;
@end

#endif  // CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_UTILS_H_
