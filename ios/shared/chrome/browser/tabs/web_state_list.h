// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHARED_CHROME_BROWSER_TABS_WEB_STATE_LIST_H_
#define IOS_SHARED_CHROME_BROWSER_TABS_WEB_STATE_LIST_H_

#import <Foundation/Foundation.h>

@protocol WebStateHandle;

// Clang format incorrectly formats the <__covariant ObjectType> used
// to declare WebStateList as a lighweight generic class. Disable it
// until the underlying issue is fixed (b/34375672).
// clang-format off

// A generic array of WebStateHandles.
@interface WebStateList<__covariant ObjectType: id<WebStateHandle>>
    : NSObject<NSFastEnumeration>

// The number of objects in the array.
@property(nonatomic, readonly) NSUInteger count;

// The first WebStateHandle in the array.
@property(nonatomic, readonly) ObjectType firstWebState;

// Inserts |webState| at the end of the array.
- (void)addWebState:(ObjectType)webState;

// Inserts |webState| into the array at |index|.
- (void)insertWebState:(ObjectType)webState atIndex:(NSUInteger)index;

// Replaces the WebStateHandle at |index| with |webState|.
- (void)replaceWebStateAtIndex:(NSUInteger)index
                  withWebState:(ObjectType)webState;

// Removes all occurrences of |webState| in the array.
- (void)removeWebState:(ObjectType)webState;

// Returns YES if |webState| is in the array.
- (BOOL)containsWebState:(ObjectType)webState;

// Returns the WebStateHandle at |index|.
- (ObjectType)webStateAtIndex:(NSUInteger)index;

// Returns the lowest index whose value is equal to |webState|.
- (NSUInteger)indexOfWebState:(ObjectType)webState;

@end

// clang-format on

#endif  // IOS_SHARED_CHROME_BROWSER_TABS_WEB_STATE_LIST_H_
