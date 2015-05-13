// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_REPORT_BACKGROUND_UPLOADER_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_REPORT_BACKGROUND_UPLOADER_H_

#import <UIKit/UIKit.h>

#include "base/ios/block_types.h"

typedef void (^BackgroundFetchCompletionBlock)(UIBackgroundFetchResult);

@interface CrashReportBackgroundUploader : NSObject

+ (instancetype)sharedInstance;

// Handler for the application delegate |performFetchWithCompletionHandler|
// message. Sends the next breakpad report if available.
+ (void)performFetchWithCompletionHandler:
        (BackgroundFetchCompletionBlock)completionHandler;

// Handler for the application delegate |handleEventsForBackgroundURLSession|
// message. Retrieves info from the URL Session.
+ (void)handleEventsForBackgroundURLSession:(NSString*)identifier
                          completionHandler:(ProceduralBlock)completionHandler;

// Returns YES if the session is a breakpad upload session.
+ (BOOL)canHandleBackgroundURLSession:(NSString*)identifier;

// Returns YES if crash reports where uploaded in the background.
+ (BOOL)hasUploadedCrashReportsInBackground;

// Resets the number of crash reports that have been uploaded.
+ (void)resetReportsUploadedInBackgroundCount;

// Flag to determine if there are any pending crash reports on startup. This is
// not an indication that there are pending crash reports at the moment this
// flag is checked.
@property(nonatomic, assign) BOOL hasPendingCrashReportsToUploadAtStartup;

@end

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_REPORT_BACKGROUND_UPLOADER_H_
