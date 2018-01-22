// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_ADAPTIVE_TOOLBAR_COORDINATOR_SUBCLASSING_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_ADAPTIVE_TOOLBAR_COORDINATOR_SUBCLASSING_H_

#import "ios/chrome/browser/ui/toolbar/adaptive/adaptive_toolbar_coordinator.h"

#import "ios/chrome/browser/ui/toolbar/clean/toolbar_type.h"

@class ToolbarButtonFactory;

// Protected interface of the AdaptiveToolbarCoordinator.
@interface AdaptiveToolbarCoordinator (Subclassing)

// Returns a button factory
- (ToolbarButtonFactory*)buttonFactoryWithType:(ToolbarType)type;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_ADAPTIVE_TOOLBAR_COORDINATOR_SUBCLASSING_H_
