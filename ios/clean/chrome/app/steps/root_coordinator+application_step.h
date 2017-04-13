// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_APP_STEPS_ROOT_COORDINATOR_APPLICATION_STEP_H_
#define IOS_CLEAN_CHROME_APP_STEPS_ROOT_COORDINATOR_APPLICATION_STEP_H_

#import "ios/clean/chrome/app/application_step.h"
#import "ios/clean/chrome/browser/ui/root/root_coordinator.h"

// Category on RootCoordinator to allow it to act as an application
// step and control the root UI for the application when is starts.
// Creates the main window and makes it key, but doesn't make it visible yet.
//  Pre:  Application phase is APPLICATION_FOREGROUNDED and the main window is
//        key.
//  Post: Application phase is (still) APPLICATION_FOREGROUNDED, the main window
//        is visible and has a root view controller set.
@interface RootCoordinator (ApplicationStep)<ApplicationStep>
@end

#endif  // IOS_CLEAN_CHROME_APP_STEPS_ROOT_COORDINATOR_APPLICATION_STEP_H_
