// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_APP_APPLICATION_STATE_H_
#define IOS_CLEAN_CHROME_APP_APPLICATION_STATE_H_

#import <UIKit/UIKit.h>

#import "ios/clean/chrome/app/application_phase.h"

namespace base {
class SupportsUserData;
}

namespace ios {
class ChromeBrowserState;
}

@protocol ApplicationStep;
@protocol URLOpening;

typedef NSMutableArray<id<ApplicationStep>> ApplicationStepArray;

// An ApplicationState object stores information about Chrome's status in the
// iOS application lifecycle and owns root-level global objects needed
// throughout the app (specifically, |browserState| and |window|).
// Additionally, an ApplicationState object keeps arrays of steps to perform
// in response to various changes in the overall application states.
// These steps are grouped into launch (used when the app is cold starting),
// termination (used when the app is shutting down), background (used when the
// app is backgrounding but staying resident), and foreground (used when the app
// is warm starting). Each group of steps is an ordered array of objects
// implementing the ApplicationStep protocol.
// Some methods (indicated below) will cause the ApplicationState to run one of
// these lists of steps; this consists of:
//   (1) Calling -canRunInState: on the first item in the list, passing in the
//       running ApplicationState object. If this returns NO, stop. Otherwise:
//   (2) Removing the first item from the list and then calling -runInState:,
//       again passing in the current state object. State objects can assume
//       that they are no longer in the step array once -runInState: is called.
//   (3) When runInState: completes, going back to (1). Since the list of steps
//       had ownership of the application step that just ran, that step will
//       then no longer be strongly held, and will thus be deallocated unless
//       it is being strongly held elsewhere.
// Steps cannot themselves store state, since they don't persist after running
// by default. Steps can write to the ApplicationState's |state| object to
// store state.
@interface ApplicationState : NSObject

// The UIApplication instance this object is storing state for.
@property(nonatomic, weak) UIApplication* application;

// The launch options dictionary for this application, set via
// -launchWithOptions:
@property(nonatomic, readonly) NSDictionary* launchOptions;

// The browser state this object stores; many launch steps are expected to
// make use of this. At least one launch step must assign to this property at
// some point in the launch sequence.
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;

// The current phase the application is in. Changing this doesn't trigger
// any launch steps; one of the phase change methods must be called to do that.
// Steps invoked during the phase change methods must update this property.
@property(nonatomic, assign) ApplicationPhase phase;

// Persistent storage for application steps that need to have objects live
// beyond the execution of the step. This should be used sparingly for objects
// that actually need to persist for the lifetime of the app.
@property(nonatomic, readonly) base::SupportsUserData* persistentState;

// The window for this application. Launch steps must create this, make it key
// and make it visible, although those three actions may be spread across
// more than one step.
@property(nonatomic, strong) UIWindow* window;

// An object that can open URLs when the application is asked to do so by the
// operating system.
@property(nonatomic, weak) id<URLOpening> URLOpener;

// Steps for each phase. Steps may add more steps to these arrays, and steps
// added in this way become strongly held by this object.
@property(nonatomic, readonly) ApplicationStepArray* launchSteps;
@property(nonatomic, readonly) ApplicationStepArray* terminationSteps;
@property(nonatomic, readonly) ApplicationStepArray* backgroundSteps;
@property(nonatomic, readonly) ApplicationStepArray* foregroundSteps;

// Phase change methods.

// Sets the launchOptions property to |launchOptions| and then runs the launch
// steps.
- (void)launchWithOptions:(NSDictionary*)launchOptions;

// Runs the launch steps.
- (void)continueLaunch;

// Runs the termination steps.
- (void)terminate;

// Runs the background steps.
- (void)background;

// Runs the foreground steps.
- (void)foreground;

@end

#endif  // IOS_CLEAN_CHROME_APP_APPLICATION_STATE_H_
