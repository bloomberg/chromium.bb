// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/app/steps/scheduled_tasks.h"

#import "ios/chrome/app/startup_tasks.h"
#import "ios/clean/chrome/app/steps/step_context.h"
#import "ios/clean/chrome/app/steps/step_features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ScheduledTasks

- (instancetype)init {
  if ((self = [super init])) {
    self.providedFeature = step_features::kScheduledTasks;
    self.requiredFeatures = @[ step_features::kForeground ];
  }
  return self;
}

- (void)runFeature:(NSString*)feature withContext:(id<StepContext>)context {
  [StartupTasks
      scheduleDeferredBrowserStateInitialization:context.browserState];
}

@end
