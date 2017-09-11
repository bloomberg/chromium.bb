// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/ntp/ntp_view_controller.h"

#import "base/ios/crb_protocol_observers.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/ntp/modal_ntp.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_bar.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_bar_item.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_view.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/clean/chrome/browser/ui/commands/ntp_commands.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NTPViewController ()<UIScrollViewDelegate, NewTabPageBarDelegate>
@property(nonatomic, strong) NewTabPageView* NTPView;
@property(nonatomic, strong) NSArray* tabBarItems;
@property(nonatomic, strong) NewTabPageBarItem* itemToDisplay;
@end

@implementation NTPViewController

@synthesize incognitoViewController = _incognitoViewController;
@synthesize bookmarksViewController = _bookmarksViewController;
@synthesize dispatcher = _dispatcher;
@synthesize homeViewController = _homeViewController;
@synthesize NTPView = _NTPView;
@synthesize recentTabsViewController = _recentTabsViewController;
@synthesize tabBarItems = _tabBarItems;
@synthesize itemToDisplay = _itemToDisplay;
@synthesize selectedNTPPanel = _selectedNTPPanel;

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
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
  scrollView.delegate = self;

  NewTabPageBar* tabBar = [[NewTabPageBar alloc] initWithFrame:CGRectZero];
  tabBar.delegate = self;
  self.NTPView = [[NewTabPageView alloc] initWithFrame:CGRectZero
                                         andScrollView:scrollView
                                             andTabBar:tabBar];
  self.NTPView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:self.NTPView];

  [NSLayoutConstraint activateConstraints:@[
    [self.NTPView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [self.NTPView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.NTPView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [self.NTPView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ]];

  DCHECK(self.tabBarItems);
  self.NTPView.tabBar.items = self.tabBarItems;
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];

  if (!self.homeViewController) {
    // Make sure scrollView is properly set up.
    [self.NTPView layoutIfNeeded];

    [self loadPanel:self.itemToDisplay];
  }
}

- (void)setHomeViewController:(UIViewController*)controller {
  _homeViewController = controller;
  if (!PresentNTPPanelModally()) {
    controller.view.frame = [self.NTPView panelFrameForItemAtIndex:1];
  } else {
    controller.view.frame = [self.NTPView panelFrameForItemAtIndex:0];
  }
  NewTabPageBarItem* item = self.NTPView.tabBar.items[1];
  item.view = controller.view;
  self.selectedNTPPanel = ntp_home::HOME_PANEL;
  [self addControllerToScrollView:controller];
}

- (void)setBookmarksViewController:(UIViewController*)controller {
  _bookmarksViewController = controller;
  controller.view.frame = [self.NTPView panelFrameForItemAtIndex:0];
  NewTabPageBarItem* item = self.NTPView.tabBar.items[0];
  item.view = controller.view;
  self.selectedNTPPanel = ntp_home::BOOKMARKS_PANEL;
  [self addControllerToScrollView:controller];
}

- (void)setRecentTabsViewController:(UIViewController*)controller {
  _recentTabsViewController = controller;
  controller.view.frame = [self.NTPView panelFrameForItemAtIndex:2];
  NewTabPageBarItem* item = self.NTPView.tabBar.items[2];
  item.view = controller.view;
  self.selectedNTPPanel = ntp_home::RECENT_TABS_PANEL;
  [self addControllerToScrollView:_recentTabsViewController];
}

- (void)setIncognitoViewController:(UIViewController*)controller {
  _incognitoViewController = controller;
  if (!PresentNTPPanelModally()) {
    controller.view.frame = [self.NTPView panelFrameForItemAtIndex:1];
    NewTabPageBarItem* item = self.NTPView.tabBar.items[1];
    item.view = controller.view;
  } else {
    controller.view.frame = [self.NTPView panelFrameForItemAtIndex:0];
  }
  self.selectedNTPPanel = ntp_home::INCOGNITO_PANEL;
  [self addControllerToScrollView:_incognitoViewController];
}

#pragma mark - Private

// Add the view controller to vc hierarchy and add the view to the scrollview.
- (void)addControllerToScrollView:(UIViewController*)controller {
  [self addChildViewController:controller];
  [self.NTPView.scrollView addSubview:controller.view];
  [controller didMoveToParentViewController:self];
}

- (void)loadPanel:(NewTabPageBarItem*)selectedItem {
  if (!PresentNTPPanelModally()) {
    NSUInteger index = [self.NTPView.tabBar.items indexOfObject:selectedItem];
    CGRect itemFrame = [self.NTPView panelFrameForItemAtIndex:index];
    CGPoint point = CGPointMake(CGRectGetMinX(itemFrame), 0);
    [self.NTPView.scrollView setContentOffset:point animated:YES];
  } else {
    if (selectedItem.identifier == ntp_home::HOME_PANEL) {
      [self.dispatcher showNTPHomePanel];
    } else if (selectedItem.identifier == ntp_home::BOOKMARKS_PANEL) {
      [self.dispatcher showNTPBookmarksPanel];
    } else if (selectedItem.identifier == ntp_home::RECENT_TABS_PANEL) {
      [self.dispatcher showNTPRecentTabsPanel];
    } else if (selectedItem.identifier == ntp_home::INCOGNITO_PANEL) {
      [self.dispatcher showNTPIncognitoPanel];
    }
  }
}

#pragma mark - NTPConsumer

- (void)setBarItems:(NSArray*)items {
  self.tabBarItems = items;
}

- (void)setFirstItemToDisplay:(NewTabPageBarItem*)itemToDisplay {
  self.itemToDisplay = itemToDisplay;
}

#pragma mark - UIScrollViewDelegate

// TODO(crbug.com/709086) Move UIScrollViewDelegate to shared object.
- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  // Position is used to track the exact X position of the scroll view, whereas
  // index is rounded to the panel that is most visible.
  CGFloat panelWidth =
      scrollView.contentSize.width / self.NTPView.tabBar.items.count;
  LayoutOffset position =
      LeadingContentOffsetForScrollView(scrollView) / panelWidth;
  NSUInteger index = round(position);

  // |scrollView| can be out of range when the frame changes.
  if (index >= self.NTPView.tabBar.items.count)
    return;

  NewTabPageBarItem* item = self.NTPView.tabBar.items[index];
  if (item.identifier == ntp_home::BOOKMARKS_PANEL &&
      !self.bookmarksViewController) {
    [self.dispatcher showNTPBookmarksPanel];
  } else if (item.identifier == ntp_home::HOME_PANEL &&
             !self.homeViewController) {
    [self.dispatcher showNTPHomePanel];
  } else if (item.identifier == ntp_home::RECENT_TABS_PANEL &&
             !self.recentTabsViewController) {
    [self.dispatcher showNTPRecentTabsPanel];
  } else if (item.identifier == ntp_home::BOOKMARKS_PANEL &&
             !self.incognitoViewController) {
    [self.dispatcher showNTPIncognitoPanel];
  }
  self.selectedNTPPanel = item.identifier;

  // If index changed, follow same path as if a tab bar item was pressed.  When
  // |index| == |position|, the panel is completely in view.
  if (index == position && self.NTPView.tabBar.selectedIndex != index) {
    NewTabPageBarItem* item = [self.NTPView.tabBar.items objectAtIndex:index];
    DCHECK(item);
    self.NTPView.tabBar.selectedIndex = index;
  }
  [self.NTPView.tabBar updateColorsForScrollView:scrollView];

  self.NTPView.tabBar.overlayPercentage =
      scrollView.contentOffset.x / scrollView.contentSize.width;
}

#pragma mark - NewTabPageBarDelegate

- (void)newTabBarItemDidChange:(NewTabPageBarItem*)selectedItem
                   changePanel:(BOOL)changePanel {
  [self loadPanel:selectedItem];
}

@end
