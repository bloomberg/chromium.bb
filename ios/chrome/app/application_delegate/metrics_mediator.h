// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_METRICS_MEDIATOR_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_METRICS_MEDIATOR_H_

#import <UIKit/UIKit.h>

@protocol StartupInformation;

@protocol BrowserInterfaceProvider;
@protocol StartupInformation;

namespace metrics_mediator {
// Key in the UserDefaults to store the date/time that the background fetch
// handler was called.
extern NSString* const kAppEnteredBackgroundDateKey;
}  // namespace metrics_mediator

// Deals with metrics, checking and updating them accordingly to to the user
// preferences.
@interface MetricsMediator : NSObject
// Returns YES if the metrics pref is enabled.  Does not take into account the
// wifi-only option or wwan state.
- (BOOL)areMetricsEnabled;
// Return YES if uploading is allowed, based on user preferences.
- (BOOL)isUploadingEnabled;
// Starts or stops the metrics service and crash report recording and/or
// uploading, based on the current user preferences. Makes sure helper
// mechanisms and the wwan state observer are set up if necessary. Must be
// called both on initialization and after user triggered preference change.
// |isUserTriggered| is used to distinguish between those cases.
- (void)updateMetricsStateBasedOnPrefsUserTriggered:(BOOL)isUserTriggered;
// Logs the duration of the cold start startup. Does nothing if there isn't a
// cold start.
+ (void)logStartupDuration:(id<StartupInformation>)startupInformation;
// Logs the number of tabs open and the start type.
+ (void)logLaunchMetricsWithStartupInformation:
            (id<StartupInformation>)startupInformation
                             interfaceProvider:(id<BrowserInterfaceProvider>)
                                                   interfaceProvider;
// Logs in UserDefaults the current date with kAppEnteredBackgroundDateKey as
// key.
+ (void)logDateInUserDefaults;
// Disables reporting in breakpad and metrics service.
+ (void)disableReporting;
// Logs that the application is in background and the number of memory warnings
// for this session.
+ (void)applicationDidEnterBackground:(NSInteger)memoryWarningCount;

@end

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_METRICS_MEDIATOR_H_
