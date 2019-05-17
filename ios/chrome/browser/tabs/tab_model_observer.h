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

// A tab was removed at the given index.
- (void)tabModel:(TabModel*)model
    didRemoveTab:(Tab*)tab
         atIndex:(NSUInteger)index;

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

// Some properties about the given tab changed, such as the URL or title.
- (void)tabModel:(TabModel*)model didChangeTab:(Tab*)tab;

// |tab| has been added to the tab model and will open. If |tab| isn't the
// active tab, |inBackground| is YES, NO otherwise.
- (void)tabModel:(TabModel*)model
    newTabWillOpen:(Tab*)tab
      inBackground:(BOOL)background;

@end

#endif  // IOS_CHROME_BROWSER_TABS_TAB_MODEL_OBSERVER_H_
