// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_CHROME_EVENT_PROCESSING_WINDOW_H_
#define CHROME_BROWSER_UI_COCOA_CHROME_EVENT_PROCESSING_WINDOW_H_

#import <Cocoa/Cocoa.h>

#import "ui/base/cocoa/underlay_opengl_hosting_window.h"

// Override NSWindow to access unhandled keyboard events (for command
// processing); subclassing NSWindow is the only method to do
// this.
@interface ChromeEventProcessingWindow : UnderlayOpenGLHostingWindow {
 @private
  BOOL redispatchingEvent_;
  BOOL eventHandled_;
}

// Sends a key event to |NSApp sendEvent:|, but also makes sure that it's not
// short-circuited to the RWHV. This is used to send keyboard events to the menu
// and the cmd-` handler if a keyboard event comes back unhandled from the
// renderer. The event must be of type |NSKeyDown|, |NSKeyUp|, or
// |NSFlagsChanged|.
// Returns |YES| if |event| has been handled.
- (BOOL)redispatchKeyEvent:(NSEvent*)event;

// See global_keyboard_shortcuts_mac.h for details on the next two functions.

// Checks if |event| is a window keyboard shortcut. If so, dispatches it to the
// window controller's |executeCommand:| and returns |YES|.
- (BOOL)handleExtraWindowKeyboardShortcut:(NSEvent*)event;

// Checks if |event| is a delayed window keyboard shortcut. If so, dispatches
// it to the window controller's |executeCommand:| and returns |YES|.
- (BOOL)handleDelayedWindowKeyboardShortcut:(NSEvent*)event;

// Checks if |event| is a browser keyboard shortcut. If so, dispatches it to the
// window controller's |executeCommand:| and returns |YES|.
- (BOOL)handleExtraBrowserKeyboardShortcut:(NSEvent*)event;

// Override, so we can handle global keyboard events.
- (BOOL)performKeyEquivalent:(NSEvent*)theEvent;

@end

#endif  // CHROME_BROWSER_UI_COCOA_CHROME_EVENT_PROCESSING_WINDOW_H_
