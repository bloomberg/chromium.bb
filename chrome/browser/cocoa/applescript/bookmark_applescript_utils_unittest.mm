// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/applescript/bookmark_applescript_utils_unittest.h"

@implementation FakeAppDelegate

@synthesize helper = helper_;

- (Profile*)defaultProfile {
  if (!helper_)
    return NULL;
  return helper_->profile();
}
@end

// Represents the current fake command that is executing.
static FakeScriptCommand* kFakeCurrentCommand;

@implementation FakeScriptCommand

- (id)init {
  if ((self = [super init])) {
    originalMethod_ = class_getClassMethod([NSScriptCommand class],
                                           @selector(currentCommand));
    alternateMethod_ = class_getClassMethod([self class],
                                            @selector(currentCommand));
    method_exchangeImplementations(originalMethod_, alternateMethod_);
    kFakeCurrentCommand = self;
  }
  return self;
}

+ (NSScriptCommand*)currentCommand {
  return kFakeCurrentCommand;
}

- (void)dealloc {
  method_exchangeImplementations(originalMethod_, alternateMethod_);
  kFakeCurrentCommand = nil;
  [super dealloc];
}

@end
