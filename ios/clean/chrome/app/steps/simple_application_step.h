// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_APP_STEPS_SIMPLE_APPLICATION_STEP_H_
#define IOS_CLEAN_CHROME_APP_STEPS_SIMPLE_APPLICATION_STEP_H_

#import <Foundation/Foundation.h>

// Note that SimpleApplicationStep doesn't conform to ApplicationStep; it's
// an abstract base class that many steps will derive from, but they will
// need to (at least) implement -runFeature:withContext.
@interface SimpleApplicationStep : NSObject
@property(nonatomic, copy) NSString* providedFeature;
// ApplicationStep properties.
@property(nonatomic) NSArray<NSString*>* providedFeatures;
@property(nonatomic) NSArray<NSString*>* requiredFeatures;
@property(nonatomic) BOOL synchronous;
@end

#endif  // IOS_CLEAN_CHROME_APP_STEPS_SIMPLE_APPLICATION_STEP_H_
