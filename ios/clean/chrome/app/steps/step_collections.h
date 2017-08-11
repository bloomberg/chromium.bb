// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_APP_STEPS_STEP_COLLECTIONS_H_
#define IOS_CLEAN_CHROME_APP_STEPS_STEP_COLLECTIONS_H_

#import <Foundation/Foundation.h>

@protocol ApplicationStep;

typedef NSArray<id<ApplicationStep>> ApplicationSteps;

@interface StepCollections : NSObject

// Returns an array containing an instance of every application step.
+ (ApplicationSteps*)allApplicationSteps;

@end

#endif  // IOS_CLEAN_CHROME_APP_STEPS_STEP_COLLECTIONS_H_
