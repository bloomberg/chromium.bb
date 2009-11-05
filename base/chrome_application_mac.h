// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CHROME_APPLICATION_MAC_H_
#define BASE_CHROME_APPLICATION_MAC_H_

#import <AppKit/AppKit.h>

#include "base/basictypes.h"

@interface CrApplication : NSApplication {
 @private
  BOOL handlingSendEvent_;
}
@property(readonly,
          getter=isHandlingSendEvent,
          nonatomic) BOOL handlingSendEvent;

+ (NSApplication*)sharedApplication;
@end

namespace chrome_application_mac {

// Controls the state of |handlingSendEvent_| in the event loop so that it is
// reset properly.
class ScopedSendingEvent {
 public:
  explicit ScopedSendingEvent(CrApplication* app);
  ~ScopedSendingEvent();

 private:
  CrApplication* app_;
  BOOL handling_;
  DISALLOW_COPY_AND_ASSIGN(ScopedSendingEvent);
};

}  // chrome_application_mac

#endif  // BASE_CHROME_APPLICATION_MAC_H_
