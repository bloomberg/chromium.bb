
// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/ntp/ntp_mediator.h"

#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/ntp/modal_ntp.h"
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

- (instancetype)initWithConsumer:(id<NTPConsumer>)consumer
                     inIncognito:(BOOL)incognito {
  self = [super init];
  if (self) {
    _consumer = consumer;
    [self setTabBarItemsForIncognito:incognito];
  }
  return self;
}

- (void)setTabBarItemsForIncognito:(BOOL)incognito {
  NSString* incognitoTitle = l10n_util::GetNSString(IDS_IOS_NEW_TAB_INCOGNITO);
  NSString* mostVisited = l10n_util::GetNSString(IDS_IOS_NEW_TAB_MOST_VISITED);
  NSString* bookmarks =
      l10n_util::GetNSString(IDS_IOS_NEW_TAB_BOOKMARKS_PAGE_TITLE_MOBILE);
  NSString* recentTabs = l10n_util::GetNSString(IDS_IOS_NEW_TAB_RECENT_TABS);

  NSMutableArray* tabBarItems = [NSMutableArray array];

  NewTabPageBarItem* incognitoItem = [NewTabPageBarItem
      newTabPageBarItemWithTitle:incognitoTitle
                      identifier:ntp_home::INCOGNITO_PANEL
                           image:[UIImage imageNamed:@"ntp_incognito"]];
  NewTabPageBarItem* homeItem = [NewTabPageBarItem
      newTabPageBarItemWithTitle:mostVisited
                      identifier:ntp_home::HOME_PANEL
                           image:[UIImage imageNamed:@"ntp_mv_search"]];
  NewTabPageBarItem* bookmarksItem = [NewTabPageBarItem
      newTabPageBarItemWithTitle:bookmarks
                      identifier:ntp_home::BOOKMARKS_PANEL
                           image:[UIImage imageNamed:@"ntp_bookmarks"]];
  NewTabPageBarItem* recentTabsItem = [NewTabPageBarItem
      newTabPageBarItemWithTitle:recentTabs
                      identifier:ntp_home::RECENT_TABS_PANEL
                           image:[UIImage imageNamed:@"ntp_opentabs"]];
  if (incognito && !PresentNTPPanelModally()) {
    [tabBarItems addObject:bookmarksItem];
    [tabBarItems addObject:incognitoItem];
  } else if (!incognito) {
    [tabBarItems addObject:bookmarksItem];
    if (!PresentNTPPanelModally()) {
      [tabBarItems addObject:homeItem];
    }
    [tabBarItems addObject:recentTabsItem];
  }

  [self.consumer setBarItems:[tabBarItems copy]];

  if (incognito) {
    [self.consumer setFirstItemToDisplay:incognitoItem];
  } else {
    [self.consumer setFirstItemToDisplay:homeItem];
  }
}

@end
