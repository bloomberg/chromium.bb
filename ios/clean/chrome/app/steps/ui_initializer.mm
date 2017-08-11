// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/app/steps/ui_initializer.h"

#import "base/logging.h"
#import "ios/clean/chrome/app/steps/step_context.h"
#import "ios/clean/chrome/app/steps/step_features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation UIInitializer

- (instancetype)init {
  if ((self = [super init])) {
    self.providedFeature = step_features::kMainWindow;
    self.requiredFeatures = @[ step_features::kForeground ];
  }
  return self;
}

- (void)runFeature:(NSString*)feature withContext:(id<StepContext>)context {
  DCHECK([context respondsToSelector:@selector(setWindow:)]);
  DCHECK([context respondsToSelector:@selector(window)]);
  context.window =
      [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
  [context.window makeKeyWindow];
}

@end
