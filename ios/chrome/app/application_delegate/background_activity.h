// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_BACKGROUND_ACTIVITY_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_BACKGROUND_ACTIVITY_H_

#import <UIKit/UIKit.h>

@protocol BrowserLauncher;
@class MetricsMediator;

// Handles the data-related methods of the ApplicationDelegate. This class has
// only class methods and should not be instantiated.
@interface BackgroundActivity : NSObject

// Class methods only.
- (instancetype)init NS_UNAVAILABLE;

// Handler for the application delegate |performFetchWithCompletionHandler|
// message. Sends the next breakpad report if available and the user permits it.
+ (void)application:(UIApplication*)application
    performFetchWithCompletionHandler:
        (void (^)(UIBackgroundFetchResult))completionHandler
                      metricsMediator:(MetricsMediator*)metricsMediator
                      browserLauncher:(id<BrowserLauncher>)browserLauncher;

// Handles Events for Background.
+ (void)handleEventsForBackgroundURLSession:(NSString*)identifier
                          completionHandler:(void (^)(void))completionHandler
                            browserLauncher:
                                (id<BrowserLauncher>)browserLauncher;

// Once the application is in foreground, reset last fetch in background time in
// NSUserDefaults.
+ (void)foregroundStarted;
@end

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_BACKGROUND_ACTIVITY_H_
