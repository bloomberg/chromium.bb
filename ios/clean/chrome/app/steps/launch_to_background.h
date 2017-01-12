// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CLEAN_CHROME_APP_STEPS_LAUNCH_TO_BACKGROUND_H_
#define IOS_CLEAN_CHROME_APP_STEPS_LAUNCH_TO_BACKGROUND_H_

#import <Foundation/Foundation.h>

#import "ios/clean/chrome/app/application_step.h"

// This file includes ApplicationStep objects that perform the steps that bring
// the application up to the background phase of launching.

// Sets up the user defaults and application bundle.
//  Pre:  Application phase is APPLICATION_BASIC.
//  Post: Application phase is (still) APPLICATION_BASIC.
@interface SetupBundleAndUserDefaults : NSObject<ApplicationStep>
@end

// Starts the ChromeMain object and sets the global WebClient object.
//  Pre:  Application phase is APPLICATION_BASIC.
//  Post: Application phase is APPLICATION_BACKGROUNDED.
@interface StartChromeMain : NSObject<ApplicationStep>
@end

// Sets the browserState property in the ApplicationState.
//  Pre:  Application phase is APPLICATION_BACKGROUNDED and a browser state
//        manager is available.
//  Post: Application phase is (still) APPLICATION_BACKGROUNDED.
@interface SetBrowserState : NSObject<ApplicationStep>
@end

#endif  // IOS_CLEAN_CHROME_APP_STEPS_LAUNCH_TO_BACKGROUND_H_
