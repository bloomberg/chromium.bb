// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CLEAN_CHROME_APP_STEPS_LAUNCH_TO_BASIC_H_
#define IOS_CLEAN_CHROME_APP_STEPS_LAUNCH_TO_BASIC_H_

#import <Foundation/Foundation.h>

#import "ios/clean/chrome/app/application_step.h"

// This file includes ApplicationStep objects that perform the very first steps
// of application launch.

// Initializes the application providers.
//  Pre:  Application phase is APPLICATION_COLD.
//  Post: Application phase is APPLICATION_BASIC.
@interface ProviderInitializer : NSObject<ApplicationStep>
@end

#endif  // IOS_CLEAN_CHROME_APP_STEPS_LAUNCH_TO_BASIC_H_
