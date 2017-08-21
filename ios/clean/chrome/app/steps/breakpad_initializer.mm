// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/app/steps/breakpad_initializer.h"

#include "base/logging.h"
#import "breakpad/src/client/ios/BreakpadController.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/clean/chrome/app/steps/step_context.h"
#import "ios/clean/chrome/app/steps/step_features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BreakpadInitializer

- (instancetype)init {
  if ((self = [super init])) {
    self.providedFeature = step_features::kBreakpad;
    self.requiredFeatures = @[];
  }
  return self;
}

- (void)runFeature:(NSString*)feature withContext:(id<StepContext>)context {
  [[BreakpadController sharedInstance] setUploadingEnabled:true];
}

@end
