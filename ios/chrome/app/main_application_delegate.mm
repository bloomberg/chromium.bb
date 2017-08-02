// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/main_application_delegate.h"

#include "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#import "ios/chrome/app/application_delegate/app_navigation.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/background_activity.h"
#import "ios/chrome/app/application_delegate/browser_launcher.h"
#import "ios/chrome/app/application_delegate/memory_warning_helper.h"
#import "ios/chrome/app/application_delegate/metrics_mediator.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/app/application_delegate/tab_switching.h"
#import "ios/chrome/app/application_delegate/url_opener.h"
#import "ios/chrome/app/application_delegate/user_activity_handler.h"
#import "ios/chrome/app/chrome_overlay_window.h"
#import "ios/chrome/app/main_application_delegate_testing.h"
#import "ios/chrome/app/main_controller.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface MainApplicationDelegate () {
  MainController* _mainController;
  // Memory helper used to log the number of memory warnings received.
  MemoryWarningHelper* _memoryHelper;
  // Metrics mediator used to check and update the metrics accordingly to
  // to the user preferences.
  MetricsMediator* _metricsMediator;
  // Browser launcher to have a global launcher.
  id<BrowserLauncher> _browserLauncher;
  // Container for startup information.
  id<StartupInformation> _startupInformation;
  // Helper to open new tabs.
  id<TabOpening> _tabOpener;
  // Handles the application stage changes.
  AppState* _appState;
  // Handles tab switcher.
  id<AppNavigation> _appNavigation;
  // Handles tab switcher.
  id<TabSwitching> _tabSwitcherProtocol;
}

@end

@implementation MainApplicationDelegate

- (instancetype)init {
  if (self = [super init]) {
    _memoryHelper = [[MemoryWarningHelper alloc] init];
    _mainController = [[MainController alloc] init];
    _metricsMediator = [[MetricsMediator alloc] init];
    [_mainController setMetricsMediator:_metricsMediator];
    _browserLauncher = _mainController;
    _startupInformation = _mainController;
    _tabOpener = _mainController;
    _appState = [[AppState alloc] initWithBrowserLauncher:_browserLauncher
                                       startupInformation:_startupInformation
                                      applicationDelegate:self];
    _tabSwitcherProtocol = _mainController;
    _appNavigation = _mainController;
    [_mainController setAppState:_appState];
  }
  return self;
}

- (UIWindow*)window {
  return [_mainController window];
}

- (void)setWindow:(UIWindow*)newWindow {
  DCHECK(newWindow);
  [_mainController setWindow:newWindow];
  // self.window has been set by this time. _appState window can now be set.
  [_appState setWindow:newWindow];
}

#pragma mark - UIApplicationDelegate methods -

#pragma mark Responding to App State Changes and System Events

// Called by the OS to create the UI for display.  The UI will not be displayed,
// even if it is ready, until this function returns.
// The absolute minimum work should be done here, to ensure that the application
// startup is fast, and the UI appears as soon as possible.
- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  // Main window must be ChromeOverlayWindow or a subclass of it.
  self.window = [[ChromeOverlayWindow alloc]
      initWithFrame:[[UIScreen mainScreen] bounds]];

  BOOL inBackground =
      [application applicationState] == UIApplicationStateBackground;
  return [_appState requiresHandlingAfterLaunchWithOptions:launchOptions
                                           stateBackground:inBackground];
}

- (void)applicationDidBecomeActive:(UIApplication*)application {
  if ([_appState isInSafeMode])
    return;

  [_appState resumeSessionWithTabOpener:_tabOpener
                            tabSwitcher:_tabSwitcherProtocol];
}

- (void)applicationWillResignActive:(UIApplication*)application {
  if ([_appState isInSafeMode])
    return;

  [_appState willResignActiveTabModel];
}

// Called when going into the background. iOS already broadcasts, so
// stakeholders can register for it directly.
- (void)applicationDidEnterBackground:(UIApplication*)application {
  [_appState
      applicationDidEnterBackground:application
                       memoryHelper:_memoryHelper
                tabSwitcherIsActive:[_mainController isTabSwitcherActive]];
}

// Called when returning to the foreground.
- (void)applicationWillEnterForeground:(UIApplication*)application {
  [_appState applicationWillEnterForeground:application
                            metricsMediator:_metricsMediator
                               memoryHelper:_memoryHelper
                                  tabOpener:_tabOpener
                              appNavigation:_appNavigation];
}

- (void)applicationWillTerminate:(UIApplication*)application {
  if ([_appState isInSafeMode])
    return;

  // Instead of adding code here, consider if it could be handled by listening
  // for  UIApplicationWillterminate.
  [_appState applicationWillTerminate:application
                applicationNavigation:_appNavigation];
}

- (void)applicationDidReceiveMemoryWarning:(UIApplication*)application {
  if ([_appState isInSafeMode])
    return;

  [_memoryHelper handleMemoryPressure];
}

#pragma mark Downloading Data in the Background

- (void)application:(UIApplication*)application
    performFetchWithCompletionHandler:
        (void (^)(UIBackgroundFetchResult))completionHandler {
  if ([_appState isInSafeMode])
    return;

  if ([application applicationState] != UIApplicationStateBackground) {
    // If this handler is called in foreground, it means it has to be activated.
    // Returning |UIBackgroundFetchResultNewData| means that the handler will be
    // called again in case of a crash.
    completionHandler(UIBackgroundFetchResultNewData);
    return;
  }

  [BackgroundActivity application:application
      performFetchWithCompletionHandler:completionHandler
                        metricsMediator:_metricsMediator
                        browserLauncher:_browserLauncher];
}

- (void)application:(UIApplication*)application
    handleEventsForBackgroundURLSession:(NSString*)identifier
                      completionHandler:(void (^)(void))completionHandler {
  if ([_appState isInSafeMode])
    return;

  [BackgroundActivity handleEventsForBackgroundURLSession:identifier
                                        completionHandler:completionHandler
                                          browserLauncher:_browserLauncher];
}

#pragma mark Continuing User Activity and Handling Quick Actions

- (BOOL)application:(UIApplication*)application
    willContinueUserActivityWithType:(NSString*)userActivityType {
  if ([_appState isInSafeMode])
    return NO;

  return
      [UserActivityHandler willContinueUserActivityWithType:userActivityType];
}

- (BOOL)application:(UIApplication*)application
    continueUserActivity:(NSUserActivity*)userActivity
      restorationHandler:(void (^)(NSArray*))restorationHandler {
  if ([_appState isInSafeMode])
    return NO;

  BOOL applicationIsActive =
      [application applicationState] == UIApplicationStateActive;

  return [UserActivityHandler continueUserActivity:userActivity
                               applicationIsActive:applicationIsActive
                                         tabOpener:_tabOpener
                                startupInformation:_startupInformation];
}

- (void)application:(UIApplication*)application
    performActionForShortcutItem:(UIApplicationShortcutItem*)shortcutItem
               completionHandler:(void (^)(BOOL succeeded))completionHandler {
  if ([_appState isInSafeMode])
    return;

  [UserActivityHandler
      performActionForShortcutItem:shortcutItem
                 completionHandler:completionHandler
                         tabOpener:_tabOpener
                startupInformation:_startupInformation
            browserViewInformation:[_mainController browserViewInformation]];
}

#pragma mark Opening a URL-Specified Resource

// Handles open URL. The registered URL Schemes are defined in project
// variables ${CHROMIUM_URL_SCHEME_x}.
// The url can either be empty, in which case the app is simply opened or
// can contain an URL that will be opened in a new tab.
- (BOOL)application:(UIApplication*)application
            openURL:(NSURL*)url
            options:(NSDictionary<NSString*, id>*)options {
  if ([_appState isInSafeMode])
    return NO;

  if (ios::GetChromeBrowserProvider()
          ->GetChromeIdentityService()
          ->HandleApplicationOpenURL(application, url, options)) {
    return YES;
  }

  BOOL applicationActive =
      [application applicationState] == UIApplicationStateActive;
  DCHECK(applicationActive || !base::ios::IsRunningOnIOS11OrLater());

  return [URLOpener openURL:url
          applicationActive:applicationActive
                    options:options
                  tabOpener:_tabOpener
         startupInformation:_startupInformation];
}

#pragma mark - chromeExecuteCommand

- (void)chromeExecuteCommand:(id)sender {
  [_mainController chromeExecuteCommand:sender];
}

#pragma mark - Testing methods

- (MainController*)mainController {
  return _mainController;
}

- (AppState*)appState {
  return _appState;
}

+ (MainController*)sharedMainController {
  return base::mac::ObjCCast<MainApplicationDelegate>(
             [[UIApplication sharedApplication] delegate])
      .mainController;
}

@end
