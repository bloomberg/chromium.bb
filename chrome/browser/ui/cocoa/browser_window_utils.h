// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_UTILS_H_
#define CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_UTILS_H_
#pragma once

#import <Cocoa/Cocoa.h>

class Browser;
struct NativeWebKeyboardEvent;

@interface BrowserWindowUtils : NSObject

// Returns YES if keyboard event should be handled.
+ (BOOL)shouldHandleKeyboardEvent:(const NativeWebKeyboardEvent&)event;

// Determines the command associated with the keyboard event.
// Returns -1 if no command found.
+ (int)getCommandId:(const NativeWebKeyboardEvent&)event;

// NSWindow must be a ChromeEventProcessingWindow.
+ (BOOL)handleKeyboardEvent:(NSEvent*)event
                   inWindow:(NSWindow*)window;
@end

#endif  // CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_UTILS_H_
