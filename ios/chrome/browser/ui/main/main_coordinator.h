// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_MAIN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_MAIN_MAIN_COORDINATOR_H_

#import "ios/chrome/browser/root_coordinator.h"

#import <Foundation/Foundation.h>

@class MainViewController;

@interface MainCoordinator : RootCoordinator

// The view controller this coordinator creates and manages.
// (This is only public while the view controller architecture is being
// refactored).
@property(nonatomic, readonly, nullable) MainViewController* mainViewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_MAIN_COORDINATOR_H_
