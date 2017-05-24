// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_TEST_BOOLEAN_OBSERVER_H_
#define IOS_WEB_VIEW_TEST_BOOLEAN_OBSERVER_H_

#import <Foundation/Foundation.h>

// Class to observe a boolean KVO compliant property. To use this object, first
// create an instance of BooleanObserver and call |setObservedObject:keyPath:|.
// After an expected change in the value of |keyPath|, ask for |lastValue| and
// compare to the expected value.
@interface BooleanObserver : NSObject

// The last value of performing |keyPath| on |object| after being notified of a
// KVO value change or null if a change has not been observed.
@property(nonatomic, nullable, readonly) NSNumber* lastValue;

// The |keyPath| of |object| being observed.
@property(nonatomic, nullable, readonly) NSString* keyPath;

// The current |object| being observed.
@property(nonatomic, nullable, readonly, weak) NSObject* object;

// Sets the |object| and |keyPath| to observe. Performing |keyPath| on |object|
// must return a BOOL value and |keyPath| must be KVO compliant.
// If |object| is null and |self.object| is nonnull, |self.object| will stop
// being observed. If |object| is nonnull, |keyPath| must be nonnull.
- (void)setObservedObject:(nullable NSObject*)object
                  keyPath:(nullable NSString*)keyPath;

@end

#endif  // IOS_WEB_VIEW_TEST_BOOLEAN_OBSERVER_H_
