// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/ntp/new_tab_page_view_controller.h"

#import "base/ios/crb_protocol_observers.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_bar.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_bar_item.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_view.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation NTPViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.title = l10n_util::GetNSString(IDS_NEW_TAB_TITLE);
  self.view.backgroundColor = [UIColor whiteColor];

  UIScrollView* scrollView = [[UIScrollView alloc] initWithFrame:CGRectZero];
  [scrollView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth |
                                   UIViewAutoresizingFlexibleHeight)];
  scrollView.pagingEnabled = YES;
  scrollView.showsHorizontalScrollIndicator = NO;
  scrollView.showsVerticalScrollIndicator = NO;
  scrollView.contentMode = UIViewContentModeScaleAspectFit;
  scrollView.bounces = YES;
  scrollView.scrollsToTop = NO;

  NewTabPageBar* tabBar = [[NewTabPageBar alloc] initWithFrame:CGRectZero];
  NewTabPageView* ntpView = [[NewTabPageView alloc] initWithFrame:CGRectZero
                                                    andScrollView:scrollView
                                                        andTabBar:tabBar];
  ntpView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:ntpView];

  [NSLayoutConstraint activateConstraints:@[
    [ntpView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [ntpView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [ntpView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [ntpView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
  ]];

  // PLACEHOLDER: this logic should move out of the UIVC.
  NSString* mostVisited = l10n_util::GetNSString(IDS_IOS_NEW_TAB_MOST_VISITED);
  NSString* bookmarks =
      l10n_util::GetNSString(IDS_IOS_NEW_TAB_BOOKMARKS_PAGE_TITLE_MOBILE);
  NSString* openTabs = l10n_util::GetNSString(IDS_IOS_NEW_TAB_RECENT_TABS);

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
  [tabBarItems addObject:mostVisitedItem];

  NewTabPageBarItem* openTabsItem = [NewTabPageBarItem
      newTabPageBarItemWithTitle:openTabs
                      identifier:NewTabPage::kOpenTabsPanel
                           image:[UIImage imageNamed:@"ntp_opentabs"]];
  [tabBarItems addObject:openTabsItem];
  tabBar.items = tabBarItems;
}

@end
