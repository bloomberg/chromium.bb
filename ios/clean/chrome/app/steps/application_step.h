// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_APP_STEPS_APPLICATION_STEP_H_
#define IOS_CLEAN_CHROME_APP_STEPS_APPLICATION_STEP_H_

#import <Foundation/Foundation.h>

@protocol StepContext;

// Objects that perform application state change steps must conform to this
// protocol.
@protocol ApplicationStep<NSObject>

// The feature(s) that running this step provides. Other steps that require
// one of these features will cause this step to be run before they run.
// Steps that do not return at least one value for this property will
// never be run.
// Steps should always return the same values for this property each time it
// is accessed.
@property(nonatomic, readonly) NSArray<NSString*>* providedFeatures;

// The features that this step requires in order to run. This array can be
// empty (or nil), indicating that this step has no dependencies.
// Steps should always return the same values for this property each time it
// is accessed.
@property(nonatomic, readonly) NSArray<NSString*>* requiredFeatures;

// YES if this step must run synchronously with other steps on the main thread.
// NO if this step can run on another thread. Steps will wait for their
// dependencies to complete regardless of which threads they are running on.
// When in doubt, return YES.
// Steps should always return the same values for this property each time it
// is accessed.
@property(nonatomic, readonly) BOOL synchronous;

// Runs the step.
// This method will be called when the step is executing. A given step
// may have this method called multiple times in its lifetime, depending on
// how the step runner is configured. It is up to the step's implementation
// to manage being run multiple times.
// A step will only be called once for each phase change, however.
// |feature| will be the feature this step was run to satisfy a requirement
// for. If a feature has only one |prividedFeature|, |feature| will always
// be that value.
// |context| is the StepContext object the step can operate on.
- (void)runFeature:(NSString*)feature withContext:(id<StepContext>)context;

@end

#endif  // IOS_CLEAN_CHROME_APP_STEPS_APPLICATION_STEP_H_
