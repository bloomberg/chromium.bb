// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_ROOT_ROOT_COORDINATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_ROOT_ROOT_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/browser_coordinator.h"
#import "ios/clean/chrome/browser/url_opening.h"

// Coordinator that runs the root container.
@interface RootCoordinator : BrowserCoordinator<URLOpening>
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_ROOT_ROOT_COORDINATOR_H_
