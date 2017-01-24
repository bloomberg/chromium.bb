// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/tab_model/tab_group.h"

#import "ios/clean/chrome/browser/web/web_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabGroup ()
@property(nonatomic) NSMutableArray<WebMediator*>* tabs;
@end

@implementation TabGroup
@synthesize tabs = _tabs;
@synthesize activeTab = _activeTab;

+ (instancetype)tabGroupWithEmptyTabCount:(NSUInteger)count
                          forBrowserState:
                              (ios::ChromeBrowserState*)browserState {
  TabGroup* group = [[TabGroup alloc] init];
  for (unsigned int i = 0; i < count; i++) {
    [group appendTab:[WebMediator webMediatorForBrowserState:browserState]];
  }
  if (count)
    group.activeTab = [group tabAtIndex:0];
  return group;
}

- (instancetype)init {
  if ((self = [super init])) {
    _tabs = [[NSMutableArray<WebMediator*> alloc] init];
  }
  return self;
}

#pragma mark - property implementations

- (BOOL)isEmpty {
  return self.count == 0;
}

- (NSUInteger)count {
  return self.tabs.count;
}

- (void)setActiveTab:(WebMediator*)activeTab {
  if ([self indexOfTab:activeTab] != NSNotFound) {
    _activeTab = activeTab;
  } else {
    _activeTab = nil;
  }
}

#pragma mark - public methods

- (WebMediator*)tabAtIndex:(NSUInteger)index {
  if (index >= self.count)
    return nil;
  return self.tabs[index];
}

- (NSUInteger)indexOfTab:(WebMediator*)tab {
  return [self.tabs indexOfObject:tab];
}

- (void)appendTab:(WebMediator*)tab {
  [self.tabs addObject:tab];
}

- (void)removeTab:(WebMediator*)tab {
  // Set the active tab to nil if it is removed. If this tab group is the
  // only object retaining |tab|, this would happen anyway, but that may not
  // always be true.
  if (self.activeTab == tab)
    self.activeTab = nil;
  [self.tabs removeObject:tab];
}

#pragma mark - NSFastEnumeration

- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState*)state
                                  objects:(id __unsafe_unretained*)buffer
                                    count:(NSUInteger)len {
  return [self.tabs countByEnumeratingWithState:state objects:buffer count:len];
}

@end
