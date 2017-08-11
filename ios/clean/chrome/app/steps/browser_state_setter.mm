// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/app/steps/browser_state_setter.h"

#include "base/logging.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#import "ios/clean/chrome/app/steps/step_context.h"
#import "ios/clean/chrome/app/steps/step_features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BrowserStateSetter

- (instancetype)init {
  if ((self = [super init])) {
    self.providedFeature = step_features::kBrowserState;
    self.requiredFeatures = @[
      step_features::kChromeMainStarted, step_features::kBundleAndDefaults
    ];
  }
  return self;
}

- (void)runFeature:(NSString*)feature withContext:(id<StepContext>)context {
  DCHECK([context respondsToSelector:@selector(setBrowserState:)]);
  context.browserState = GetApplicationContext()
                             ->GetChromeBrowserStateManager()
                             ->GetLastUsedBrowserState();
}

@end
