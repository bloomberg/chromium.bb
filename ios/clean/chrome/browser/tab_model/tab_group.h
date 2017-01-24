// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_TAB_MODEL_TAB_GROUP_H_
#define IOS_CLEAN_CHROME_BROWSER_TAB_MODEL_TAB_GROUP_H_

#import <Foundation/Foundation.h>

namespace ios {
class ChromeBrowserState;
}

@class WebMediator;

@interface TabGroup : NSObject<NSFastEnumeration>

// PLACEHOLDER: Convenience method for generating a tab group with empty tabs.
+ (instancetype)tabGroupWithEmptyTabCount:(NSUInteger)count
                          forBrowserState:
                              (ios::ChromeBrowserState*)browserState;

// The "active" tab in the group. Can be nil. If the active tab is removed,
// this property becomes nil. If the active tab is set to one not in the group,
// this property becomes nil (even if it wasn't nil before).
@property(nonatomic, weak) WebMediator* activeTab;

// NO if the model has at least one tab.
@property(nonatomic, readonly, getter=isEmpty) BOOL empty;

// The number of tabs in the model.
@property(nonatomic, readonly) NSUInteger count;

// Accesses the tab at the given index.
- (WebMediator*)tabAtIndex:(NSUInteger)index;
- (NSUInteger)indexOfTab:(WebMediator*)tab;

// Adds |tab| to the receiver.
- (void)appendTab:(WebMediator*)tab;

// Removes |tab| from the receiver.
- (void)removeTab:(WebMediator*)tab;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_TAB_MODEL_TAB_GROUP_H_
