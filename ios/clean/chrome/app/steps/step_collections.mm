// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/app/steps/step_collections.h"

#import "ios/clean/chrome/app/steps/breakpad_initializer.h"
#import "ios/clean/chrome/app/steps/browser_state_setter.h"
#import "ios/clean/chrome/app/steps/bundle_and_defaults_configurator.h"
#import "ios/clean/chrome/app/steps/chrome_main.h"
#import "ios/clean/chrome/app/steps/foregrounder.h"
#import "ios/clean/chrome/app/steps/provider_initializer.h"
#import "ios/clean/chrome/app/steps/root_coordinator_initializer.h"
#import "ios/clean/chrome/app/steps/scheduled_tasks.h"
#import "ios/clean/chrome/app/steps/ui_initializer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation StepCollections

+ (ApplicationSteps*)allApplicationSteps {
  return @[
    [[BrowserStateSetter alloc] init],
    [[BundleAndDefaultsConfigurator alloc] init], [[ChromeMain alloc] init],
    [[Foregrounder alloc] init], [[ProviderInitializer alloc] init],
    [[UIInitializer alloc] init], [[ScheduledTasks alloc] init],
    [[RootCoordinatorInitializer alloc] init],
    [[BreakpadInitializer alloc] init]
  ];
}

@end
