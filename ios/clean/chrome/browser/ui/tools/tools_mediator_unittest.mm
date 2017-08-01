// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tools/tools_mediator.h"

#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/ui/toolbar/test/toolbar_test_navigation_manager.h"
#import "ios/chrome/browser/ui/toolbar/test/toolbar_test_web_state.h"
#import "ios/chrome/browser/ui/tools_menu/tools_menu_configuration.h"
#import "ios/clean/chrome/browser/ui/tools/tools_consumer.h"
#import "ios/clean/chrome/browser/ui/tools/tools_mediator_private.h"
#import "ios/clean/chrome/browser/ui/tools/tools_menu_item.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class ToolsMediatorTest : public PlatformTest {
 public:
  ToolsMediatorTest()
      : consumer_(OCMProtocolMock(@protocol(ToolsConsumer))),
        configuration_(
            [[ToolsMenuConfiguration alloc] initWithDisplayView:nil]) {
    auto navigation_manager = base::MakeUnique<ToolbarTestNavigationManager>();
    test_web_state_.SetNavigationManager(std::move(navigation_manager));
  }

 protected:
  ToolsMediator* mediator_;
  id consumer_;
  ToolsMenuConfiguration* configuration_;
  ToolbarTestWebState test_web_state_;
};

TEST_F(ToolsMediatorTest, TestShowOverFlowControls) {
  configuration_.inTabSwitcher = YES;
  mediator_ = [[ToolsMediator alloc] initWithConsumer:consumer_
                                        configuration:configuration_];

  [[consumer_ verify] setToolsMenuItems:[OCMArg any]];
  [[consumer_ verify] setDisplayOverflowControls:NO];
}

TEST_F(ToolsMediatorTest, TestHideOverFlowControls) {
  configuration_.inTabSwitcher = NO;

  mediator_ = [[ToolsMediator alloc] initWithConsumer:consumer_
                                        configuration:configuration_];

  [[consumer_ verify] setToolsMenuItems:[OCMArg any]];
  [[consumer_ verify] setDisplayOverflowControls:YES];
}

TEST_F(ToolsMediatorTest, TestMenuItemsForNonTabSwitcherNonIncognito) {
  configuration_.inTabSwitcher = NO;
  configuration_.inIncognito = NO;

  mediator_ = [[ToolsMediator alloc] initWithConsumer:consumer_
                                        configuration:configuration_];

  EXPECT_EQ(7ul, [mediator_.menuItemsArray count]);
}

TEST_F(ToolsMediatorTest, TestMenuItemsForNonTabSwitcherIncognito) {
  configuration_.inTabSwitcher = NO;
  configuration_.inIncognito = YES;

  mediator_ = [[ToolsMediator alloc] initWithConsumer:consumer_
                                        configuration:configuration_];

  EXPECT_EQ(7ul, [mediator_.menuItemsArray count]);
}

TEST_F(ToolsMediatorTest, TestMenuItemsForTabSwitcherIncognito) {
  configuration_.inTabSwitcher = YES;
  configuration_.inIncognito = YES;

  mediator_ = [[ToolsMediator alloc] initWithConsumer:consumer_
                                        configuration:configuration_];

  ToolsMenuItem* closeAllTabsItem = mediator_.menuItemsArray[2];
  EXPECT_NSEQ(@"Close All Incognito Tabs", closeAllTabsItem.title);
  EXPECT_EQ(5ul, [mediator_.menuItemsArray count]);
}

TEST_F(ToolsMediatorTest, TestMenuItemsForTabSwitcherNonIncognito) {
  configuration_.inTabSwitcher = YES;
  configuration_.inIncognito = NO;

  mediator_ = [[ToolsMediator alloc] initWithConsumer:consumer_
                                        configuration:configuration_];

  ToolsMenuItem* closeAllTabsItem = mediator_.menuItemsArray[2];
  EXPECT_NSEQ(@"Close All Tabs", closeAllTabsItem.title);
  EXPECT_EQ(5ul, [mediator_.menuItemsArray count]);
}

TEST_F(ToolsMediatorTest, TestEnabledMenuItemsForOpenTabs) {
  configuration_.inTabSwitcher = YES;
  configuration_.inIncognito = NO;
  configuration_.noOpenedTabs = NO;

  mediator_ = [[ToolsMediator alloc] initWithConsumer:consumer_
                                        configuration:configuration_];

  ToolsMenuItem* closeAllTabsItem = mediator_.menuItemsArray[2];
  EXPECT_NSEQ(@"Close All Tabs", closeAllTabsItem.title);
  EXPECT_EQ(true, closeAllTabsItem.enabled);
}

TEST_F(ToolsMediatorTest, TestEnabledMenuItemsForNoOpenTabs) {
  configuration_.inTabSwitcher = YES;
  configuration_.inIncognito = NO;
  configuration_.noOpenedTabs = YES;

  mediator_ = [[ToolsMediator alloc] initWithConsumer:consumer_
                                        configuration:configuration_];

  ToolsMenuItem* closeAllTabsItem = mediator_.menuItemsArray[2];
  EXPECT_NSEQ(@"Close All Tabs", closeAllTabsItem.title);
  EXPECT_EQ(false, closeAllTabsItem.enabled);
}

TEST_F(ToolsMediatorTest, TestEnabledMenuItemsForOpenTabsIncognito) {
  configuration_.inTabSwitcher = YES;
  configuration_.inIncognito = YES;
  configuration_.noOpenedTabs = NO;

  mediator_ = [[ToolsMediator alloc] initWithConsumer:consumer_
                                        configuration:configuration_];

  ToolsMenuItem* closeAllTabsItem = mediator_.menuItemsArray[2];
  EXPECT_NSEQ(@"Close All Incognito Tabs", closeAllTabsItem.title);
  EXPECT_EQ(true, closeAllTabsItem.enabled);
}

TEST_F(ToolsMediatorTest, TestEnabledMenuItemsForNoOpenTabsIncognito) {
  configuration_.inTabSwitcher = YES;
  configuration_.inIncognito = YES;
  configuration_.noOpenedTabs = YES;

  mediator_ = [[ToolsMediator alloc] initWithConsumer:consumer_
                                        configuration:configuration_];

  ToolsMenuItem* closeAllTabsItem = mediator_.menuItemsArray[2];
  EXPECT_NSEQ(@"Close All Incognito Tabs", closeAllTabsItem.title);
  EXPECT_EQ(false, closeAllTabsItem.enabled);
}

TEST_F(ToolsMediatorTest, TestEnabledMenuItemsForInNTP) {
  configuration_.inTabSwitcher = NO;
  configuration_.inIncognito = NO;
  configuration_.inNewTabPage = YES;

  mediator_ = [[ToolsMediator alloc] initWithConsumer:consumer_
                                        configuration:configuration_];

  ToolsMenuItem* findInPageItem = mediator_.menuItemsArray[4];
  EXPECT_NSEQ(@"Find in Page…", findInPageItem.title);
  EXPECT_EQ(false, findInPageItem.enabled);
}

TEST_F(ToolsMediatorTest, TestEnabledMenuItemsForNotInNTP) {
  configuration_.inTabSwitcher = NO;
  configuration_.inIncognito = NO;
  configuration_.inNewTabPage = NO;

  mediator_ = [[ToolsMediator alloc] initWithConsumer:consumer_
                                        configuration:configuration_];

  ToolsMenuItem* findInPageItem = mediator_.menuItemsArray[4];
  EXPECT_NSEQ(@"Find in Page…", findInPageItem.title);
  EXPECT_EQ(true, findInPageItem.enabled);
}

TEST_F(ToolsMediatorTest, TestDontUpdateConsumerLoadingState) {
  configuration_.inTabSwitcher = NO;
  [[consumer_ reject] setIsLoading:YES];
  [[consumer_ reject] setIsLoading:NO];

  mediator_ = [[ToolsMediator alloc] initWithConsumer:consumer_
                                        configuration:configuration_];
}

TEST_F(ToolsMediatorTest, TestDontUpdateConsumerLoadingStateInTabSwitcher) {
  configuration_.inTabSwitcher = YES;
  [[consumer_ reject] setIsLoading:YES];
  [[consumer_ reject] setIsLoading:NO];

  mediator_ = [[ToolsMediator alloc] initWithConsumer:consumer_
                                        configuration:configuration_];
}

TEST_F(ToolsMediatorTest, TestUpdateConsumerLoadingState) {
  configuration_.inTabSwitcher = NO;
  mediator_ = [[ToolsMediator alloc] initWithConsumer:consumer_
                                        configuration:configuration_];
  test_web_state_.SetLoading(false);
  mediator_.webState = &test_web_state_;
  [[consumer_ verify] setIsLoading:NO];
  test_web_state_.SetLoading(true);
  [[consumer_ verify] setIsLoading:YES];
}

TEST_F(ToolsMediatorTest, TestUpdateConsumerLoadingStateInverse) {
  configuration_.inTabSwitcher = NO;
  mediator_ = [[ToolsMediator alloc] initWithConsumer:consumer_
                                        configuration:configuration_];
  test_web_state_.SetLoading(true);
  mediator_.webState = &test_web_state_;
  [[consumer_ verify] setIsLoading:YES];
  test_web_state_.SetLoading(false);
  [[consumer_ verify] setIsLoading:NO];
}

TEST_F(ToolsMediatorTest, TestUpdateConsumerLoadingStateInTabSwitcher) {
  configuration_.inTabSwitcher = YES;
  [[consumer_ reject] setIsLoading:NO];
  [[consumer_ reject] setIsLoading:YES];

  mediator_ = [[ToolsMediator alloc] initWithConsumer:consumer_
                                        configuration:configuration_];
  test_web_state_.SetLoading(false);
  mediator_.webState = &test_web_state_;
  test_web_state_.SetLoading(true);
}

TEST_F(ToolsMediatorTest, TestUpdateConsumerLoadingStateInverseInTabSwitcher) {
  configuration_.inTabSwitcher = YES;
  [[consumer_ reject] setIsLoading:NO];
  [[consumer_ reject] setIsLoading:YES];

  mediator_ = [[ToolsMediator alloc] initWithConsumer:consumer_
                                        configuration:configuration_];
  test_web_state_.SetLoading(true);
  mediator_.webState = &test_web_state_;
  test_web_state_.SetLoading(false);
}

}  // namespace
