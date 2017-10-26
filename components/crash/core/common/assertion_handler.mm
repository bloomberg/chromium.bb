// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/crash/core/common/assertion_handler.h"

#import <Foundation/Foundation.h>

#include <cstdarg>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"

@interface CrNSAssertionHandler : NSAssertionHandler
@end

@implementation CrNSAssertionHandler

- (void)handleFailureInMethod:(SEL)selector
                       object:(id)object
                         file:(NSString*)fileName
                   lineNumber:(NSInteger)line
                  description:(NSString*)format, ... {
  va_list args;
  va_start(args, format);

  base::scoped_nsobject<NSString> error(
      [[NSString alloc] initWithFormat:format arguments:args]);

  va_end(args);

  LOG(FATAL) << base::SysNSStringToUTF8(error);
}

- (void)handleFailureInFunction:(NSString*)functionName
                           file:(NSString*)fileName
                     lineNumber:(NSInteger)line
                    description:(NSString*)format, ... {
  va_list args;
  va_start(args, format);

  base::scoped_nsobject<NSString> error(
      [[NSString alloc] initWithFormat:format arguments:args]);

  va_end(args);

  LOG(FATAL) << base::SysNSStringToUTF8(error);
}

@end

namespace crash_reporter {

void InstallNSAssertionHandlerOnCurrentThread() {
  NSMutableDictionary* threadDictionary =
      [[NSThread currentThread] threadDictionary];
  if (!base::mac::ObjCCast<CrNSAssertionHandler>(
          [threadDictionary objectForKey:NSAssertionHandlerKey])) {
    base::scoped_nsobject<CrNSAssertionHandler> handler(
        [[CrNSAssertionHandler alloc] init]);
    [threadDictionary setObject:handler forKey:NSAssertionHandlerKey];
  }
}

}  // namespace crash_reporter
