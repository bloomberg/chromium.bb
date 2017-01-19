// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/background_activity.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/time/time.h"
#import "ios/chrome/app/application_delegate/browser_launcher.h"
#import "ios/chrome/app/application_delegate/metrics_mediator.h"
#import "ios/chrome/browser/crash_report/crash_report_background_uploader.h"
#include "ios/chrome/browser/metrics/first_user_action_recorder.h"
#import "ios/chrome/browser/metrics/previous_session_info.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Key of the UMA Startup.BackgroundFetchTimeInterval histogram.
const char* const kUMAMobileBackgroundFetchTimeInterval =
    "Startup.BackgroundFetchTimeInterval";
// Key in the UserDefaults to store the date/time that the application was last
// launched for a background fetch.
NSString* const kLastBackgroundFetch = @"LastBackgroundFetch";
}  // namespace

@implementation BackgroundActivity

- (instancetype)init {
  NOTREACHED();
  return nil;
}

#pragma mark - Public class methods.

+ (void)application:(UIApplication*)application
    performFetchWithCompletionHandler:
        (void (^)(UIBackgroundFetchResult))completionHandler
                      metricsMediator:(MetricsMediator*)metricsMediator
                      browserLauncher:(id<BrowserLauncher>)browserLauncher {
  [browserLauncher startUpBrowserToStage:INITIALIZATION_STAGE_BACKGROUND];

  // TODO(crbug.com/616436): Check if this can be moved to MetricsMediator.
  base::RecordAction(base::UserMetricsAction("BackgroundFetchCalled"));
  NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
  base::Time lastTime =
      base::Time::FromDoubleT([userDefaults doubleForKey:kLastBackgroundFetch]);
  base::Time now = base::Time::Now();
  if (!lastTime.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES_100(kUMAMobileBackgroundFetchTimeInterval,
                                 now - lastTime);
  }
  [userDefaults setDouble:now.ToDoubleT() forKey:kLastBackgroundFetch];

  // The crashes concerning an old version are obsolete. To reduce data usage,
  // these crash reports are not uploaded.
  if (![metricsMediator areMetricsEnabled] ||
      [[PreviousSessionInfo sharedInstance] isFirstSessionAfterUpgrade]) {
    [application setMinimumBackgroundFetchInterval:
                     UIApplicationBackgroundFetchIntervalNever];
    completionHandler(UIBackgroundFetchResultNoData);
    return;
  }

  if (![metricsMediator isUploadingEnabled]) {
    completionHandler(UIBackgroundFetchResultFailed);
    return;
  }
  [CrashReportBackgroundUploader
      performFetchWithCompletionHandler:completionHandler];
}

+ (void)handleEventsForBackgroundURLSession:(NSString*)identifier
                          completionHandler:(void (^)(void))completionHandler
                            browserLauncher:
                                (id<BrowserLauncher>)browserLauncher {
  [browserLauncher startUpBrowserToStage:INITIALIZATION_STAGE_BACKGROUND];
  if ([CrashReportBackgroundUploader
          canHandleBackgroundURLSession:identifier]) {
    [CrashReportBackgroundUploader
        handleEventsForBackgroundURLSession:identifier
                          completionHandler:completionHandler];
  } else {
    completionHandler();
  }
}

+ (void)foregroundStarted {
  [[NSUserDefaults standardUserDefaults]
      removeObjectForKey:kLastBackgroundFetch];
}

@end
