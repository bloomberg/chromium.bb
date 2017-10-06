// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/app/steps/step_features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace step_features {

NSString* const kProviders = @"providers";
NSString* const kBundleAndDefaults = @"bundleAndDefaults";
NSString* const kChromeMainStarted = @"chromeMainStarted";
NSString* const kChromeMainStopped = @"chromeMainStopped";
NSString* const kForeground = @"foreground";
NSString* const kBrowserState = @"browserState";
NSString* const kBrowserStateInitialized = @"browserStateInit";
NSString* const kMainWindow = @"mainWindow";
NSString* const kScheduledTasks = @"scheduledTasks";
NSString* const kRootCoordinatorStarted = @"rootCoordinatorStarted";
NSString* const kBreakpad = @"breakpad";

}  // namespace step_features
