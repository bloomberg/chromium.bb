// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_APP_STEPS_PHASED_STEP_RUNNER_H_
#define IOS_CLEAN_CHROME_APP_STEPS_PHASED_STEP_RUNNER_H_

#import <Foundation/Foundation.h>

#import "ios/clean/chrome/app/steps/application_step.h"
#import "ios/clean/chrome/app/steps/step_context.h"

// This class implements a generic state machine that will execute one or more
// "steps" in response to changes in its |phase| property. (The numeric |phase|
// property is defined as part of the StepContext protocol.)
//
// Steps, their dependencies, and the mechanisms by which they are executed
// are defined in terms of "features", "steps" and "jobs".
//
// A "feature" in this context is a NSString that refers to some action that a
// step performs, or some global structure that it initializes. Features are
// intentionally indirect labels for steps; they allow for dependencies to
// be expressed between the steps while keeping the step implementations
// encapsulated.
//
// A "step" is an object that conforms to ApplicationStep. A step defines one
// or more features it provides, and may define any number of features it
// requires as well. The step runner will use the dependencies expressed by
// steps to determine the order that steps need to execute in.
//
// Once a step is added to the step runner, the step object will persist for
// the lifetime of the step runner. If a step is run in multiple phases, it
// is the same step object that is run each time. This allows a step to maintain
// internal state.
//
// A "job" is the internal object that is used to encapsulate the steps that
// need to run each time the step runner's phase changes. Jobs are created,
// run at most once, and then disposed of.
//
// "Running a step" consists of calling the step's -runFeature:withContext:
// method, passing in the feature that the step was run for, and the
// current StepContext (the task runner itself). StepContext exposes the
// step runner's phase as readwrite, which means that during the execution of
// the steps triggered by a phase change, the phase might be changed to a new
// value.
//
// If this happens, as soon as the phase is set to another value, all queued
// jobs are cancelled, and new jobs are triggered for the new phase change.
//
// By defualt, steps are executed on the main thread. Steps that advertise
// a |synchronous| property value of |NO| will execute on another thread.
// The -setPhase: method of the phasedStepRunner will wait for all jobs to
// complete before returning, regardless of the threads they run on.
@interface PhasedStepRunner : NSObject<StepContext>

// Adds |step| as one of the application steps that this runner knows about.
// (The step will only be executed if it is required to do so in the
// dependency graph for a phase change).
// It is an error if |step| provides a feature that has already been provided
// by a previously-added step.
- (void)addStep:(id<ApplicationStep>)step;

// Adds all of the steps in |steps|.
- (void)addSteps:(NSArray<id<ApplicationStep>>*)steps;

// Adds |feature| as a requirement when the receiver's phase changes to
// |phase|.
- (void)requireFeature:(NSString*)feature forPhase:(NSUInteger)phase;
@end

#endif  // IOS_CLEAN_CHROME_APP_STEPS_PHASED_STEP_RUNNER_H_
