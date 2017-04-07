
// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/ntp/ntp_mediator.h"

#import "ios/chrome/browser/ui/ntp/new_tab_page_bar_item.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_controller.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/clean/chrome/browser/ui/ntp/ntp_consumer.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NTPMediator ()
@property(nonatomic, strong) id<NTPConsumer> consumer;
@end

@implementation NTPMediator

@synthesize consumer = _consumer;

- (instancetype)initWithConsumer:(id<NTPConsumer>)consumer {
  self = [super init];
  if (self) {
    _consumer = consumer;
    [self setTabBarItems];
  }
  return self;
}

- (void)setTabBarItems {
  NSString* mostVisited = l10n_util::GetNSString(IDS_IOS_NEW_TAB_MOST_VISITED);
  NSString* bookmarks =
      l10n_util::GetNSString(IDS_IOS_NEW_TAB_BOOKMARKS_PAGE_TITLE_MOBILE);
  NSString* recentTabs = l10n_util::GetNSString(IDS_IOS_NEW_TAB_RECENT_TABS);

  NSMutableArray* tabBarItems = [NSMutableArray array];

  NewTabPageBarItem* mostVisitedItem = [NewTabPageBarItem
      newTabPageBarItemWithTitle:mostVisited
                      identifier:NewTabPage::kMostVisitedPanel
                           image:[UIImage imageNamed:@"ntp_mv_search"]];
  NewTabPageBarItem* bookmarksItem = [NewTabPageBarItem
      newTabPageBarItemWithTitle:bookmarks
                      identifier:NewTabPage::kBookmarksPanel
                           image:[UIImage imageNamed:@"ntp_bookmarks"]];
  [tabBarItems addObject:bookmarksItem];
  if (IsIPadIdiom()) {
    [tabBarItems addObject:mostVisitedItem];
  }

  NewTabPageBarItem* recentTabsItem = [NewTabPageBarItem
      newTabPageBarItemWithTitle:recentTabs
                      identifier:NewTabPage::kOpenTabsPanel
                           image:[UIImage imageNamed:@"ntp_opentabs"]];
  [tabBarItems addObject:recentTabsItem];

  [self.consumer setBarItems:[tabBarItems copy]];
}

@end
