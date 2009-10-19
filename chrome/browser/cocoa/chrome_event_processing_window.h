// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_CHROME_EVENT_PROCESSING_WINDOW_H_
#define CHROME_BROWSER_COCOA_CHROME_EVENT_PROCESSING_WINDOW_H_

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"

// Override NSWindow to access unhandled keyboard events (for command
// processing); subclassing NSWindow is the only method to do
// this.
@interface ChromeEventProcessingWindow : NSWindow {
 @private
  BOOL redispatchingEvent_;
}

// Returns |YES| if |event| has been shortcircuited and should not be processed
// further.
- (BOOL)shortcircuitEvent:(NSEvent*)event;

// Sends an event to |NSApp sendEvent:|, but also makes sure that it's not
// short-circuited to the RWHV. This is used to send keyboard events to the menu
// and the cmd-` handler if a keyboard event comes back unhandled from the
// renderer.
- (void)redispatchEvent:(NSEvent*)event;

// See global_keyboard_shortcuts_mac.h for details on the next two functions.

// Checks if |event| is a window keyboard shortcut. If so, dispatches it to the
// window controller's |executeCommand:| and returns |YES|.
- (BOOL)handleExtraWindowKeyboardShortcut:(NSEvent*)event;

// Checks if |event| is a browser keyboard shortcut. If so, dispatches it to the
// window controller's |executeCommand:| and returns |YES|.
- (BOOL)handleExtraBrowserKeyboardShortcut:(NSEvent*)event;

// Override, so we can handle global keyboard events.
- (BOOL)performKeyEquivalent:(NSEvent*)theEvent;

@end

#endif  // CHROME_BROWSER_COCOA_CHROME_EVENT_PROCESSING_WINDOW_H_
