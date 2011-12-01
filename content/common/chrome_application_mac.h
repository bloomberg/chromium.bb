// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CHROME_APPLICATION_MAC_H_
#define CONTENT_COMMON_CHROME_APPLICATION_MAC_H_
#pragma once

#if defined(__OBJC__)

#import <AppKit/AppKit.h>

#include "base/basictypes.h"
#import "base/mac/scoped_sending_event.h"

@interface CrApplication : NSApplication<CrAppControlProtocol> {
 @private
  BOOL handlingSendEvent_;
}
- (BOOL)isHandlingSendEvent;

// Unconditionally clears |handlingSendEvent_|.  This should not be
// used except in recovering from some sort of exceptional condition.
- (void)clearIsHandlingSendEvent;

+ (NSApplication*)sharedApplication;
@end

#endif  // defined(__OBJC__)

namespace chrome_application_mac {

// To be used to instantiate CrApplication from C++ code.
void RegisterCrApp();

}  // namespace chrome_application_mac

#endif  // CONTENT_COMMON_CHROME_APPLICATION_MAC_H_
