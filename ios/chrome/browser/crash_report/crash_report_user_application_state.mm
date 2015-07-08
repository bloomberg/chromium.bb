// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/crash_report_user_application_state.h"

namespace {
NSString* const kBrowserState = @"user_application_state";
}

@implementation CrashReportUserApplicationState

+ (CrashReportUserApplicationState*)sharedInstance {
  static CrashReportUserApplicationState* instance =
      [[CrashReportUserApplicationState alloc] initWithKey:kBrowserState];
  return instance;
}

@end
