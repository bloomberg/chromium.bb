// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/app/steps/provider_initializer.h"

#import "ios/chrome/app/startup/provider_registration.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/ui/webui/chrome_web_ui_ios_controller_factory.h"
#import "ios/clean/chrome/app/steps/step_features.h"
#include "ios/web/public/webui/web_ui_ios_controller_factory.h"

@protocol StepContext;
#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif
@implementation ProviderInitializer
- (instancetype)init {
  if ((self = [super init])) {
    self.providedFeature = step_features::kProviders;
  }
  return self;
}

- (void)runFeature:(NSString*)feature withContext:(id<StepContext>)context {
  web::WebUIIOSControllerFactory::RegisterFactory(
      ChromeWebUIIOSControllerFactory::GetInstance());
  [ProviderRegistration registerProviders];
}

@end
