// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/app/app_delegate.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/app/startup/chrome_app_startup_parameters.h"
#import "ios/clean/chrome/app/application_state.h"
#import "ios/clean/chrome/browser/url_opening.h"
#import "ios/testing/perf/startupLoggers.h"
#include "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface AppDelegate ()
@property(nonatomic, strong) ApplicationState* applicationState;
@end

@implementation AppDelegate

@synthesize applicationState = _applicationState;

#pragma mark - UIApplicationDelegate (app state changes and system events)

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  startup_loggers::RegisterAppDidFinishLaunchingTime();
  self.applicationState = [[ApplicationState alloc] init];
  [self.applicationState configure];
  self.applicationState.launchOptions = launchOptions;

  switch (application.applicationState) {
    case UIApplicationStateBackground:
      // The app is launching in the background.
      self.applicationState.phase = APPLICATION_BACKGROUNDED;
      break;
    case UIApplicationStateInactive:
      // The app is launching in the foreground but hasn't become active yet.
      self.applicationState.phase = APPLICATION_FOREGROUNDED;
      break;
    case UIApplicationStateActive:
      // This should never happen.
      NOTREACHED() << "Application unexpectedly active in "
                   << base::SysNSStringToUTF8(NSStringFromSelector(_cmd));
  }

  return YES;
}

- (void)applicationDidBecomeActive:(UIApplication*)application {
  startup_loggers::RegisterAppDidBecomeActiveTime();
}

- (void)applicationWillResignActive:(UIApplication*)application {
}

- (void)applicationDidEnterBackground:(UIApplication*)application {
}

- (void)applicationWillEnterForeground:(UIApplication*)application {
}

- (void)applicationWillTerminate:(UIApplication*)application {
  self.applicationState.phase = APPLICATION_TERMINATING;
}

- (void)applicationDidReceiveMemoryWarning:(UIApplication*)application {
}

#pragma mark - UIApplicationDelegate (background dowloading)

- (void)application:(UIApplication*)application
    performFetchWithCompletionHandler:
        (void (^)(UIBackgroundFetchResult))completionHandler {
}

- (void)application:(UIApplication*)application
    handleEventsForBackgroundURLSession:(NSString*)identifier
                      completionHandler:(void (^)())completionHandler {
}

#pragma mark - UIApplicationDelegate (user activity and quick actions)

- (BOOL)application:(UIApplication*)application
    willContinueUserActivityWithType:(NSString*)userActivityType {
  return NO;
}

- (BOOL)application:(UIApplication*)application
    continueUserActivity:(NSUserActivity*)userActivity
      restorationHandler:(void (^)(NSArray*))restorationHandler {
  return NO;
}

- (void)application:(UIApplication*)application
    performActionForShortcutItem:(UIApplicationShortcutItem*)shortcutItem
               completionHandler:(void (^)(BOOL))completionHandler {
}

#pragma mark - UIApplicationDelegate (opening URL-specified resources)

- (BOOL)application:(UIApplication*)application
            openURL:(NSURL*)url
            options:(NSDictionary<UIApplicationOpenURLOptionsKey, id>*)options {
  NSString* sourceApplication =
      options[UIApplicationOpenURLOptionsSourceApplicationKey];
  ChromeAppStartupParameters* params = [ChromeAppStartupParameters
      newChromeAppStartupParametersWithURL:url
                     fromSourceApplication:sourceApplication];
  [self.applicationState.URLOpener
      openURL:net::NSURLWithGURL(params.externalURL)];
  return YES;
}

@end
