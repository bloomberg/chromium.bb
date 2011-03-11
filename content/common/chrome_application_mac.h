// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CHROME_APPLICATION_MAC_H_
#define CONTENT_COMMON_CHROME_APPLICATION_MAC_H_
#pragma once

#if defined(__OBJC__)

#import <AppKit/AppKit.h>

#include "base/basictypes.h"
#include "base/message_pump_mac.h"
#include "base/scoped_nsobject.h"

// Event hooks must implement this protocol.
@protocol CrApplicationEventHookProtocol
- (void)hookForEvent:(NSEvent*)theEvent;
@end


@interface CrApplication : NSApplication<CrAppProtocol> {
 @private
  BOOL handlingSendEvent_;
  // Array of objects implementing the CrApplicationEventHookProtocol
  scoped_nsobject<NSMutableArray> eventHooks_;
}
- (BOOL)isHandlingSendEvent;

// Add or remove an event hook to be called for every sendEvent:
// that the application receives.  These handlers are called before
// the normal [NSApplication sendEvent:] call is made.

// This is not a good alternative to a nested event loop.  It should
// be used only when normal event logic and notification breaks down
// (e.g. when clicking outside a canBecomeKey:NO window to "switch
// context" out of it).
- (void)addEventHook:(id<CrApplicationEventHookProtocol>)hook;
- (void)removeEventHook:(id<CrApplicationEventHookProtocol>)hook;

+ (NSApplication*)sharedApplication;
@end

namespace chrome_application_mac {

// Controls the state of |handlingSendEvent_| in the event loop so that it is
// reset properly.
class ScopedSendingEvent {
 public:
  ScopedSendingEvent();
  ~ScopedSendingEvent();

 private:
  CrApplication* app_;
  BOOL handling_;
  DISALLOW_COPY_AND_ASSIGN(ScopedSendingEvent);
};

}  // namespace chrome_application_mac

#endif  // defined(__OBJC__)

namespace chrome_application_mac {

// To be used to instantiate CrApplication from C++ code.
void RegisterCrApp();

}  // namespace chrome_application_mac

#endif  // CONTENT_COMMON_CHROME_APPLICATION_MAC_H_
