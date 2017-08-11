// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_APP_STEPS_STEP_CONTEXT_H_
#define IOS_CLEAN_CHROME_APP_STEPS_STEP_CONTEXT_H_

#import <UIKit/UIKit.h>

namespace ios {
class ChromeBrowserState;
}

@class BrowserCoordinator;

@protocol URLOpening;

// Objects that provide a context for ApplicationSteps to run in should conform
// to this protocol.
@protocol StepContext<NSObject>

// The current phase that the object running the step is in. Generally
// speaking, the phase value determines which steps will be run.
// This property is writable, so steps can change the current phase.
@property(nonatomic) NSUInteger phase;

@property(nonatomic, weak) id<URLOpening> URLOpener;

@optional
// Properties with contextual information for the execution of application
// steps.

// The main browserState object for the application
@property(nonatomic) ios::ChromeBrowserState* browserState;

// A dictionary with keys and values as provided by the UIApplicationDelegate
// -application:(did|will)FinishLaunchingWithOptions: methods.
@property(nonatomic, readonly) NSDictionary* launchOptions;

// The main UIWindow for the application.
@property(nonatomic, strong) UIWindow* window;

@end

#endif  // IOS_CLEAN_CHROME_APP_STEPS_STEP_CONTEXT_H_
