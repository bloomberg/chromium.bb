// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include <memory>

#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/sessions/test_session_service.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/tabs/tab_strip_controller.h"
#import "ios/chrome/browser/ui/tabs/tab_strip_view.h"
#import "ios/chrome/browser/ui/tabs/tab_view.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ArrayBackedTabModel : TabModel {
 @private
  NSMutableArray* tabsForTesting_;
}

@end

@implementation ArrayBackedTabModel

- (instancetype)initWithSessionWindow:(SessionWindowIOS*)window
                       sessionService:(SessionServiceIOS*)service
                         browserState:(ios::ChromeBrowserState*)browserState {
  if ((self = [super initWithSessionWindow:window
                            sessionService:service
                              browserState:browserState])) {
    tabsForTesting_ = [[NSMutableArray alloc] initWithCapacity:5];
  }
  return self;
}

- (void)addTabForTesting:(Tab*)tab {
  [tabsForTesting_ addObject:tab];
}

- (Tab*)tabAtIndex:(NSUInteger)index {
  return (Tab*)[tabsForTesting_ objectAtIndex:index];
}

- (NSUInteger)indexOfTab:(Tab*)tab {
  return [tabsForTesting_ indexOfObject:tab];
}

- (BOOL)isEmpty {
  return [tabsForTesting_ count] == 0;
}

- (NSUInteger)count {
  return [tabsForTesting_ count];
}

// Pass along fast enumeration calls to the tab array.
- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState*)state
                                  objects:(id __unsafe_unretained*)stackbuf
                                    count:(NSUInteger)len {
  return [tabsForTesting_ countByEnumeratingWithState:state
                                              objects:stackbuf
                                                count:len];
}

- (void)closeTabAtIndex:(NSUInteger)index {
  DCHECK(index < [tabsForTesting_ count]);
  [self closeTab:[tabsForTesting_ objectAtIndex:index]];
}

@end

namespace {

class TabStripControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    if (!IsIPadIdiom())
      return;

    // Need a ChromeBrowserState for the tab model.
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();

    // Setup mock TabModel, sessionService, and Tabs.
    TestSessionService* test_service = [[TestSessionService alloc] init];
    real_tab_model_ = [[ArrayBackedTabModel alloc]
        initWithSessionWindow:nil
               sessionService:test_service
                 browserState:chrome_browser_state_.get()];
    id tabModel = [OCMockObject partialMockForObject:real_tab_model_];
    id tab1 = [OCMockObject mockForClass:[Tab class]];
    id tab2 = [OCMockObject mockForClass:[Tab class]];

    // Populate the tab model.
    [real_tab_model_ addTabForTesting:tab1];
    [real_tab_model_ addTabForTesting:tab2];

    // Stub methods for TabModel.
    [[[tabModel stub] andDo:^(NSInvocation* invocation) {
      // Return the raw pointer to the C++ object.
      ios::ChromeBrowserState* browser_state = chrome_browser_state_.get();
      [invocation setReturnValue:&browser_state];
    }] browserState];
    [[tabModel stub] addObserver:[OCMArg any]];
    [[tabModel stub] removeObserver:[OCMArg any]];

    // Stub methods for Tabs.
    [[[tab1 stub] andReturn:@"Tab Title 1"] title];
    [[[tab2 stub] andReturn:@"Tab Title 2"] title];
    [[[tab1 stub] andReturn:nil] favicon];
    [[[tab2 stub] andReturn:nil] favicon];
    [[tab1 stub] close];
    [[tab2 stub] close];

    tabModel_ = tabModel;
    tab1_ = tab1;
    tab2_ = tab2;
    controller_ =
        [[TabStripController alloc] initWithTabModel:(TabModel*)tabModel_
                                               style:TabStrip::kStyleDark];

    // Force the view to load.
    UIWindow* window = [[UIWindow alloc] initWithFrame:CGRectZero];
    [window addSubview:[controller_ view]];
    window_ = window;
  }
  void TearDown() override {
    if (!IsIPadIdiom())
      return;
    [real_tab_model_ browserStateDestroyed];
  }

  web::TestWebThreadBundle thread_bundle_;
  OCMockObject* tab1_;
  OCMockObject* tab2_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  OCMockObject* tabModel_;
  TabStripController* controller_;
  UIWindow* window_;
  ArrayBackedTabModel* real_tab_model_;
};

TEST_F(TabStripControllerTest, LoadAndDisplay) {
  if (!IsIPadIdiom())
    return;

  // If this doesn't crash, we're good.
  ASSERT_OCMOCK_VERIFY(tabModel_);
  ASSERT_OCMOCK_VERIFY(tab1_);
  ASSERT_OCMOCK_VERIFY(tab2_);

  // There should be two TabViews and one new tab button nested within the
  // parent view (which contains exactly one scroll view).
  EXPECT_EQ(3U,
            [[[[[controller_ view] subviews] objectAtIndex:0] subviews] count]);
}

}  // namespace
