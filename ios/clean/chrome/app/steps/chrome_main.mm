// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/app/steps/chrome_main.h"

#include "base/memory/ptr_util.h"
#include "ios/chrome/app/startup/ios_chrome_main.h"
#import "ios/chrome/browser/web/chrome_web_client.h"
#import "ios/clean/chrome/app/steps/step_features.h"
#include "ios/web/public/web_client.h"

@protocol StepContext;

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ChromeMain {
  std::unique_ptr<IOSChromeMain> _chromeMain;
}

- (instancetype)init {
  if ((self = [super init])) {
    self.providedFeatures = @[
      step_features::kChromeMainStarted, step_features::kChromeMainStopped
    ];
    self.requiredFeatures =
        @[ step_features::kProviders, step_features::kBreakpad ];
  }
  return self;
}

- (void)runFeature:(NSString*)feature withContext:(id<StepContext>)context {
  if ([feature isEqualToString:step_features::kChromeMainStarted]) {
    web::SetWebClient(new ChromeWebClient());
    _chromeMain = base::MakeUnique<IOSChromeMain>();
  } else {
    DCHECK([feature isEqualToString:step_features::kChromeMainStopped]);
    _chromeMain.reset();
  }
}

@end
