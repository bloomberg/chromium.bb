// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/clean/chrome/app/app_delegate.h"

#import "ios/clean/chrome/app/application_state.h"
#import "ios/clean/chrome/app/steps/launch_to_background.h"
#import "ios/clean/chrome/app/steps/launch_to_basic.h"
#import "ios/clean/chrome/app/steps/launch_to_foreground.h"
#import "ios/clean/chrome/app/steps/tab_grid_coordinator+application_step.h"

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
  self.applicationState = [[ApplicationState alloc] init];
  self.applicationState.application = application;
  [self configureApplicationState];

  [self.applicationState launchWithOptions:launchOptions];
  return YES;
}

- (void)applicationDidBecomeActive:(UIApplication*)application {
}

- (void)applicationWillResignActive:(UIApplication*)application {
}

- (void)applicationDidEnterBackground:(UIApplication*)application {
}

- (void)applicationWillEnterForeground:(UIApplication*)application {
}

- (void)applicationWillTerminate:(UIApplication*)application {
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
  [self.applicationState.URLOpener openURL:url];
  return YES;
}

#pragma mark - Private methods

// Configures the application state for application launch by setting the launch
// steps.
// Future architecture/refactoring note: configuring the application state in
// this way is outside the scope of responsibility of the object as defined in
// the header file. The correct solution is probably a helper object that can
// perform all of the configuration necessary, and that can be adjusted as
// needed.
- (void)configureApplicationState {
  [self.applicationState.launchSteps addObjectsFromArray:@[
    [[ProviderInitializer alloc] init],
    [[SetupBundleAndUserDefaults alloc] init],
    [[StartChromeMain alloc] init],
    [[SetBrowserState alloc] init],
    [[BeginForegrounding alloc] init],
    [[PrepareForUI alloc] init],
    [[CompleteForegrounding alloc] init],
    [[TabGridCoordinator alloc] init],
  ]];
}

@end
