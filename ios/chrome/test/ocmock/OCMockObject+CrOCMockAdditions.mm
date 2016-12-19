// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/ocmock/OCMockObject+CrOCMockAdditions.h"

@implementation OCMockObject (CrOCMockAdditions)

- (BOOL)isStubbed:(SEL)selector {
  for (OCMockRecorder* recorder in recorders) {
    NSMethodSignature* signature = [self methodSignatureForSelector:selector];
    NSInvocation* invocation =
        [NSInvocation invocationWithMethodSignature:signature];
    [invocation setSelector:selector];
    if ([recorder matchesInvocation:invocation]) {
      return YES;
    }
  }
  return NO;
}

- (void)removeStub:(SEL)selector {
  OCMockRecorder* stubRecorder = nil;
  for (OCMockRecorder* recorder in recorders) {
    NSMethodSignature* signature = [self methodSignatureForSelector:selector];
    NSInvocation* invocation =
        [NSInvocation invocationWithMethodSignature:signature];
    [invocation setSelector:selector];
    if ([recorder matchesInvocation:invocation]) {
      stubRecorder = recorder;
      break;
    }
  }
  if (stubRecorder) {
    [recorders removeObject:stubRecorder];
  }
}

@end
