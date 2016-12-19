// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CHROME_APP_STEPS_TAB_GRID_COORDINATOR_APPLICATION_STEP_H_
#define IOS_CHROME_APP_STEPS_TAB_GRID_COORDINATOR_APPLICATION_STEP_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/app/application_step.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_coordinator.h"

// Category on TabGridCoordinator to allow it to act as an application step
// and control the root UI for the application when is starts.
// Creates the main window and makes it key, but doesn't make it visible yet.
//  Pre:  Application phase is APPLICATION_FOREGROUNDED and the main window is
//        key.
//  Post: Application phase is (still) APPLICATION_FOREGROUNDED, the main window
//        is visible and has a root view controller set.
@interface TabGridCoordinator (ApplicationStep)<ApplicationStep>
@end

#endif  // IOS_CHROME_APP_STEPS_TAB_GRID_COORDINATOR_APPLICATION_STEP_H_
