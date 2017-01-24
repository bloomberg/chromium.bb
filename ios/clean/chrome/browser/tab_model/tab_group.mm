// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/tab_model/tab_group.h"

#import "ios/clean/chrome/browser/web/web_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TabGroup
@synthesize activeTab = _activeTab;

+ (instancetype)tabGroupWithEmptyTabCount:(NSUInteger)count
                          forBrowserState:
                              (ios::ChromeBrowserState*)browserState {
  TabGroup* group = [[TabGroup alloc] init];
  for (unsigned int i = 0; i < count; i++) {
    [group addWebState:[WebMediator webMediatorForBrowserState:browserState]];
  }
  if (count)
    group.activeTab = [group webStateAtIndex:0];
  return group;
}

#pragma mark - property implementations

- (BOOL)isEmpty {
  return self.count == 0;
}

- (void)setActiveTab:(WebMediator*)activeTab {
  if ([self indexOfWebState:activeTab] != NSNotFound) {
    _activeTab = activeTab;
  } else {
    _activeTab = nil;
  }
}

#pragma mark - superclass methods

- (void)replaceWebStateAtIndex:(NSUInteger)index
                  withWebState:(WebMediator*)webState {
  // Set the active tab to nil if it is replaced. If this tab group is the
  // only object retaining |tab|, this would happen anyway, but that may not
  // always be true.
  if (self.activeTab == webState)
    self.activeTab = nil;
  [super replaceWebStateAtIndex:index withWebState:webState];
}

- (void)removeWebState:(WebMediator*)webState {
  // Set the active tab to nil if it is removed. If this tab group is the
  // only object retaining |tab|, this would happen anyway, but that may not
  // always be true.
  if (self.activeTab == webState)
    self.activeTab = nil;
  [super removeWebState:webState];
}

@end
