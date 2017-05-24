// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tools/tools_mediator.h"

#import "ios/clean/chrome/browser/ui/tools/tools_consumer.h"
#import "ios/clean/chrome/browser/ui/tools/tools_mediator_private.h"
#import "ios/clean/chrome/browser/ui/tools/tools_menu_item.h"
#import "ios/shared/chrome/browser/ui/tools_menu/tools_menu_configuration.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class ToolsMediatorTest : public PlatformTest {
 protected:
  ToolsMediator* mediator_;
};

TEST_F(ToolsMediatorTest, TestShowOverFlowControls) {
  id consumer = OCMProtocolMock(@protocol(ToolsConsumer));
  id configuration = OCMClassMock([ToolsMenuConfiguration class]);
  OCMStub([configuration isInTabSwitcher]).andReturn(YES);

  mediator_ = [[ToolsMediator alloc] initWithConsumer:consumer
                                     andConfiguration:configuration];

  [[consumer verify] setToolsMenuItems:[OCMArg any]];
  [[consumer verify] setDisplayOverflowControls:NO];
}

TEST_F(ToolsMediatorTest, TestHideOverFlowControls) {
  id consumer = OCMProtocolMock(@protocol(ToolsConsumer));
  id configuration = OCMClassMock([ToolsMenuConfiguration class]);
  OCMStub([configuration isInTabSwitcher]).andReturn(NO);

  mediator_ = [[ToolsMediator alloc] initWithConsumer:consumer
                                     andConfiguration:configuration];

  [[consumer verify] setToolsMenuItems:[OCMArg any]];
  [[consumer verify] setDisplayOverflowControls:YES];
}

TEST_F(ToolsMediatorTest, TestMenuItemsForNonTabSwitcherNonIncognito) {
  id consumer = OCMProtocolMock(@protocol(ToolsConsumer));
  id configuration = OCMClassMock([ToolsMenuConfiguration class]);
  OCMStub([configuration isInTabSwitcher]).andReturn(NO);
  OCMStub([configuration isInIncognito]).andReturn(NO);

  mediator_ = [[ToolsMediator alloc] initWithConsumer:consumer
                                     andConfiguration:configuration];

  EXPECT_EQ(7ul, [mediator_.menuItemsArray count]);
}

TEST_F(ToolsMediatorTest, TestMenuItemsForNonTabSwitcherIncognito) {
  id consumer = OCMProtocolMock(@protocol(ToolsConsumer));
  id configuration = OCMClassMock([ToolsMenuConfiguration class]);
  OCMStub([configuration isInTabSwitcher]).andReturn(NO);
  OCMStub([configuration isInIncognito]).andReturn(YES);

  mediator_ = [[ToolsMediator alloc] initWithConsumer:consumer
                                     andConfiguration:configuration];

  EXPECT_EQ(7ul, [mediator_.menuItemsArray count]);
}

TEST_F(ToolsMediatorTest, TestMenuItemsForTabSwitcherNonIncognito) {
  id consumer = OCMProtocolMock(@protocol(ToolsConsumer));
  id configuration = OCMClassMock([ToolsMenuConfiguration class]);
  OCMStub([configuration isInTabSwitcher]).andReturn(YES);
  OCMStub([configuration isInIncognito]).andReturn(YES);

  mediator_ = [[ToolsMediator alloc] initWithConsumer:consumer
                                     andConfiguration:configuration];

  ToolsMenuItem* closeAllTabsItem = mediator_.menuItemsArray[2];
  EXPECT_EQ(@"Close All Incognito Tabs", closeAllTabsItem.title);
  EXPECT_EQ(5ul, [mediator_.menuItemsArray count]);
}

TEST_F(ToolsMediatorTest, TestMenuItemsForTabSwitcherIncognito) {
  id consumer = OCMProtocolMock(@protocol(ToolsConsumer));
  id configuration = OCMClassMock([ToolsMenuConfiguration class]);
  OCMStub([configuration isInTabSwitcher]).andReturn(YES);
  OCMStub([configuration isInIncognito]).andReturn(NO);

  mediator_ = [[ToolsMediator alloc] initWithConsumer:consumer
                                     andConfiguration:configuration];

  ToolsMenuItem* closeAllTabsItem = mediator_.menuItemsArray[2];
  EXPECT_EQ(@"Close All Tabs", closeAllTabsItem.title);
  EXPECT_EQ(5ul, [mediator_.menuItemsArray count]);
}

}  // namespace
