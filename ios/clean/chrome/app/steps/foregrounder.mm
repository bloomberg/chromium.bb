// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/app/steps/foregrounder.h"

#include "ios/chrome/browser/application_context.h"
#import "ios/clean/chrome/app/steps/step_features.h"

@protocol StepContext;
#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif
@implementation Foregrounder
- (instancetype)init {
  if ((self = [super init])) {
    self.providedFeature = step_features::kForeground;
    self.requiredFeatures = @[
      step_features::kBundleAndDefaults, step_features::kChromeMainStarted,
      step_features::kBrowserState, step_features::kChromeMainStarted
    ];
  }
  return self;
}

- (void)runFeature:(NSString*)feature withContext:(id<StepContext>)context {
  GetApplicationContext()->OnAppEnterForeground();
}

@end
