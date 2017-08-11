// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_APP_STEPS_BUNDLE_AND_DEFAULTS_CONFIGURATOR_H_
#define IOS_CLEAN_CHROME_APP_STEPS_BUNDLE_AND_DEFAULTS_CONFIGURATOR_H_

#import <Foundation/Foundation.h>

#import "ios/clean/chrome/app/steps/application_step.h"
#import "ios/clean/chrome/app/steps/simple_application_step.h"

@interface BundleAndDefaultsConfigurator
    : SimpleApplicationStep<ApplicationStep>
@end

#endif  // IOS_CLEAN_CHROME_APP_STEPS_BUNDLE_AND_DEFAULTS_CONFIGURATOR_H_
