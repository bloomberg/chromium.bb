// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_UNAVAILABLE_FEATURE_DIALOGS_UNAVAILABLE_FEATURE_DIALOG_COORDINATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_UNAVAILABLE_FEATURE_DIALOGS_UNAVAILABLE_FEATURE_DIALOG_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "ios/clean/chrome/browser/ui/dialogs/dialog_coordinator.h"

// A DialogCoordinator for displaying alerts when an in-development feature is
// unavailable.
@interface UnavailableFeatureDialogCoordinator : DialogCoordinator

// Designated initializer for an unavailable feature called |featureName|.
// |featureName| is expected to be non-empty.
- (nullable instancetype)initWithFeatureName:(nonnull NSString*)featureName
    NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_UNAVAILABLE_FEATURE_DIALOGS_UNAVAILABLE_FEATURE_DIALOG_COORDINATOR_H_
