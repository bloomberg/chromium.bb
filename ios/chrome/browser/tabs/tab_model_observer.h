// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_TAB_MODEL_OBSERVER_H_
#define IOS_CHROME_BROWSER_TABS_TAB_MODEL_OBSERVER_H_

#import <Foundation/Foundation.h>

@class Tab;
@class TabModel;

// Observers implement these methods to be alerted to changes in the model.
// These methods are all optional.
@protocol TabModelObserver<NSObject>
@optional

// A tab was inserted at the given index.
- (void)tabModel:(TabModel*)model
    didInsertTab:(Tab*)tab
         atIndex:(NSUInteger)index
    inForeground:(BOOL)fg;

// The given tab will be removed.
- (void)tabModel:(TabModel*)model willRemoveTab:(Tab*)tab;

// A tab was removed at the given index.
- (void)tabModel:(TabModel*)model
    didRemoveTab:(Tab*)tab
         atIndex:(NSUInteger)index;

// A tab was moved to the given index.  |fromIndex| is guaranteed to be
// different than |toIndex| (moves to the same index do not trigger
// notifications).  This method is only called for moves that change the
// relative order of tabs (for example, closing the first tab decrements all tab
// indexes by one, but will not call this observer method).
- (void)tabModel:(TabModel*)model
      didMoveTab:(Tab*)tab
       fromIndex:(NSUInteger)fromIndex
         toIndex:(NSUInteger)toIndex;

// A Tab was replaced in the model, at the given index.
- (void)tabModel:(TabModel*)model
    didReplaceTab:(Tab*)oldTab
          withTab:(Tab*)newTab
          atIndex:(NSUInteger)index;

// A tab was activated.
- (void)tabModel:(TabModel*)model
    didChangeActiveTab:(Tab*)newTab
           previousTab:(Tab*)previousTab
               atIndex:(NSUInteger)index;

// The number of Tabs in this TabModel changed.
- (void)tabModelDidChangeTabCount:(TabModel*)model;

// Some properties about the given tab changed, such as the URL or title.
- (void)tabModel:(TabModel*)model didChangeTab:(Tab*)tab;

// The |tab|'s snapshot was changed to |image|.
- (void)tabModel:(TabModel*)model
    didChangeTabSnapshot:(Tab*)tab
               withImage:image;

@end

#endif  // IOS_CHROME_BROWSER_TABS_TAB_MODEL_OBSERVER_H_
