// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_APPLICATION_MAC_H_
#define CHROME_BROWSER_CHROME_BROWSER_APPLICATION_MAC_H_
#pragma once

#ifdef __OBJC__

#import <AppKit/AppKit.h>

#include <vector>

#import "base/mac/scoped_sending_event.h"
#import "base/memory/scoped_nsobject.h"
#import "base/message_pump_mac.h"
#include "base/synchronization/lock.h"

// Event hooks must implement this protocol.
@protocol CrApplicationEventHookProtocol
- (void)hookForEvent:(NSEvent*)theEvent;
@end

@interface BrowserCrApplication : NSApplication<CrAppProtocol,
                                                CrAppControlProtocol> {
 @private
  BOOL handlingSendEvent_;
  BOOL cyclingWindows_;

  // Array of objects implementing CrApplicationEventHookProtocol.
  scoped_nsobject<NSMutableArray> eventHooks_;

  // App's previous key windows. Most recent key window is last.
  // Does not include current key window. Elements of this vector are weak
  // references.
  std::vector<NSWindow*> previousKeyWindows_;

  // Guards previousKeyWindows_.
  base::Lock previousKeyWindowsLock_;
}

// Our implementation of |-terminate:| only attempts to terminate the
// application, i.e., begins a process which may lead to termination. This
// method cancels that process.
- (void)cancelTerminate:(id)sender;

// Add or remove an event hook to be called for every sendEvent:
// that the application receives.  These handlers are called before
// the normal [NSApplication sendEvent:] call is made.

// This is not a good alternative to a nested event loop.  It should
// be used only when normal event logic and notification breaks down
// (e.g. when clicking outside a canBecomeKey:NO window to "switch
// context" out of it).
- (void)addEventHook:(id<CrApplicationEventHookProtocol>)hook;
- (void)removeEventHook:(id<CrApplicationEventHookProtocol>)hook;

// Keep track of the previous key windows and whether windows are being
// cycled for use in determining whether a Panel window can become the
// key window.
- (NSWindow*)previousKeyWindow;
- (BOOL)isCyclingWindows;
@end

namespace chrome_browser_application_mac {

// Bin for unknown exceptions. Exposed for testing purposes.
extern const size_t kUnknownNSException;

// Returns the histogram bin for |exception| if it is one we track
// specifically, or |kUnknownNSException| if unknown.  Exposed for testing
// purposes.
size_t BinForException(NSException* exception);

// Use UMA to track exception occurance. Exposed for testing purposes.
void RecordExceptionWithUma(NSException* exception);

}  // namespace chrome_browser_application_mac

#endif  // __OBJC__

namespace chrome_browser_application_mac {

// To be used to instantiate BrowserCrApplication from C++ code.
void RegisterBrowserCrApp();

// Calls -[NSApp terminate:].
void Terminate();

// Cancels a termination started by |Terminate()|.
void CancelTerminate();

}  // namespace chrome_browser_application_mac

#endif  // CHROME_BROWSER_CHROME_BROWSER_APPLICATION_MAC_H_
