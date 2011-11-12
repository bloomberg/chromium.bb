// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/common/chrome_application_mac.h"

#include "base/logging.h"

@interface CrApplication ()
- (void)setHandlingSendEvent:(BOOL)handlingSendEvent;
@end

@implementation CrApplication
// Initialize NSApplication using the custom subclass.  Check whether NSApp
// was already initialized using another class, because that would break
// some things.
+ (NSApplication*)sharedApplication {
  NSApplication* app = [super sharedApplication];
  if (![NSApp isKindOfClass:self]) {
    DLOG(ERROR) << "NSApp should be of type " << [[self className] UTF8String]
                << ", not " << [[NSApp className] UTF8String];
    DCHECK(false) << "NSApp is of wrong type";
  }
  return app;
}

- (BOOL)isHandlingSendEvent {
  return handlingSendEvent_;
}

- (void)setHandlingSendEvent:(BOOL)handlingSendEvent {
  handlingSendEvent_ = handlingSendEvent;
}

- (void)clearIsHandlingSendEvent {
  [self setHandlingSendEvent:NO];
}

- (void)sendEvent:(NSEvent*)event {
  content::mac::ScopedSendingEvent sendingEventScoper;
  [super sendEvent:event];
}

@end

namespace chrome_application_mac {

void RegisterCrApp() {
  [CrApplication sharedApplication];
}

}  // namespace chrome_application_mac
