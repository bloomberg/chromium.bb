// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_APP_STATE_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_APP_STATE_H_

#import <UIKit/UIKit.h>

@class AppState;
@protocol BrowserLauncher;
@class SceneState;
@class MainApplicationDelegate;
@class MemoryWarningHelper;
@class MetricsMediator;
@protocol StartupInformation;
@protocol TabOpening;
@protocol TabSwitching;

@protocol AppStateObserver <NSObject>

@optional

// Called when the first scene becomes active.
- (void)appState:(AppState*)appState
    firstSceneActivated:(SceneState*)sceneState;

// Called after the app exits safe mode.
- (void)appStateDidExitSafeMode:(AppState*)appState;

@end

// Represents the application state and responds to application state changes
// and system events.
@interface AppState : NSObject

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)
initWithBrowserLauncher:(id<BrowserLauncher>)browserLauncher
     startupInformation:(id<StartupInformation>)startupInformation
    applicationDelegate:(MainApplicationDelegate*)applicationDelegate
    NS_DESIGNATED_INITIALIZER;

// YES if the user has ever interacted with the application. May be NO if the
// application has been woken up by the system for background work.
@property(nonatomic, readonly) BOOL userInteracted;

// Current foreground active for the application, if any. Some scene's window
// otherwise. For legacy use cases only, use scene windows instead.
@property(nonatomic, readonly) UIWindow* window;

// When multiwindow is unavailable, this is the only scene state. It is created
// by the app delegate.
@property(nonatomic, strong) SceneState* mainSceneState;

// Saves the launchOptions to be used from -newTabFromLaunchOptions. If the
// application is in background, initialize the browser to basic. If not, launch
// the browser.
// Returns whether additional delegate handling should be performed (call to
// -performActionForShortcutItem or -openURL by the system for example)
- (BOOL)requiresHandlingAfterLaunchWithOptions:(NSDictionary*)launchOptions
                               stateBackground:(BOOL)stateBackground;

// Whether the application is in Safe Mode.
- (BOOL)isInSafeMode;

// Logs duration of the session in the main tab model and records that chrome is
// no longer in cold start.
- (void)willResignActiveTabModel;

// Called when the application is getting terminated. It stops all outgoing
// requests, config updates, clears the device sharing manager and stops the
// mainChrome instance.
- (void)applicationWillTerminate:(UIApplication*)application;

// Resumes the session: reinitializing metrics and opening new tab if necessary.
// User sessions are defined in terms of BecomeActive/ResignActive so that
// session boundaries include things like turning the screen off or getting a
// phone call, not just switching apps.
- (void)resumeSessionWithTabOpener:(id<TabOpening>)tabOpener
                       tabSwitcher:(id<TabSwitching>)tabSwitcher;

// Called when going into the background. iOS already broadcasts, so
// stakeholders can register for it directly.
- (void)applicationDidEnterBackground:(UIApplication*)application
                         memoryHelper:(MemoryWarningHelper*)memoryHelper
              incognitoContentVisible:(BOOL)incognitoContentVisible;

// Called when returning to the foreground. Resets and uploads the metrics.
// Starts the browser to foreground if needed.
- (void)applicationWillEnterForeground:(UIApplication*)application
                       metricsMediator:(MetricsMediator*)metricsMediator
                          memoryHelper:(MemoryWarningHelper*)memoryHelper
                             tabOpener:(id<TabOpening>)tabOpener;

// Sets the return value for -didFinishLaunchingWithOptions that determines if
// UIKit should make followup delegate calls such as
// -performActionForShortcutItem or -openURL.
- (void)launchFromURLHandled:(BOOL)URLHandled;

// Returns the foreground and active scene, if there is one.
- (SceneState*)foregroundActiveScene;

// Returns a list of all connected scenes.
- (NSArray<SceneState*>*)connectedScenes;

// Adds an observer to this app state. The observers will be notified about
// app state changes per AppStateObserver protocol.
- (void)addObserver:(id<AppStateObserver>)observer;
// Removes the observer. It's safe to call this at any time, including from
// AppStateObserver callbacks.
- (void)removeObserver:(id<AppStateObserver>)observer;

@end

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_APP_STATE_H_
