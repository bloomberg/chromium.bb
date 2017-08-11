// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_APP_APPLICATION_STATE_H_
#define IOS_CLEAN_CHROME_APP_APPLICATION_STATE_H_

#import <UIKit/UIKit.h>

#import "ios/clean/chrome/app/application_phase.h"
#import "ios/clean/chrome/app/steps/phased_step_runner.h"

// This class manages the application's transitions from one state to another,
// and keeps pointers to some global objects.
// This class is a specialization of PhasedStepRunner (and thus it conforms to
// StepContext). It uses the ApplicationPhase enum as the type of its |phase|
// property, and all of its phase transitions should be expressed in terms of
// those phases.
//
// See PhasedStepRunner's header documentation for information on how steps
// are ordered and executed, and the distinctions between "features", "steps",
// and "jobs" in this context.
//
// This class's -configure method will create all of the neccessary application
// steps, and define the steps to execute in response to each phase change.
//
// The general process to add new steps that will be handled by this object is:
//  (1) Implement the step as class in chrome/app/steps/, either subclassing
//      SimpleApplicationStep or just conforming to ApplicationStep.
//  (2) Add a feature string for the feature the step enables to step_features.h
//  (3) Be sure your implementation sets this feature as its |providedFeature|.
//  (4) Be sure your implementation sets the features it requires as its
//      |requiredFeatures|.
//  (5) If other features directly require your feature, be sure to add your
//      feature to their |requiredFeatures| arrays.
//  (6) Update [StepCollections +allApplicationSteps] to include an instance of
//      your step.
//  (7) If your step must run in one or more phases, and isn't dependent on
//      a feature already (transitively) required for that phase, add the
//      necessare -requireFeature:forPhase: calls into this class's -configure
//      method.

@interface ApplicationState : PhasedStepRunner

// StepContext property redeclared readwrite.
@property(nonatomic, readwrite, copy) NSDictionary* launchOptions;

// StepContext property redeclared with enum type.
@property(nonatomic) ApplicationPhase phase;

// Optional StepContext properties explicitly declared here so
// direct users of this class don't need to check for support.
@property(nonatomic) ios::ChromeBrowserState* browserState;

@property(nonatomic, strong) UIWindow* window;

- (void)configure;

@end

#endif  // IOS_CLEAN_CHROME_APP_APPLICATION_STATE_H_
