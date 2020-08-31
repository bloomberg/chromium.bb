// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//
// SearchInChromeIntent.m
//
// This file was automatically generated and should not be edited.
//

#import "SearchInChromeIntent.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SearchInChromeIntent

@end

@interface SearchInChromeIntentResponse ()

@property(readwrite, NS_NONATOMIC_IOSONLY)
    SearchInChromeIntentResponseCode code;

@end

@implementation SearchInChromeIntentResponse

@synthesize code = _code;

- (instancetype)initWithCode:(SearchInChromeIntentResponseCode)code
                userActivity:(nullable NSUserActivity*)userActivity {
  self = [super init];
  if (self) {
    _code = code;
    self.userActivity = userActivity;
  }
  return self;
}

@end
