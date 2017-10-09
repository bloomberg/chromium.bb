// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/share_to_data_builder.h"

#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/ui/activity_services/share_to_data.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#import "ios/web/web_state/web_state_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/test/ios/ui_image_test_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using web::NavigationManagerImpl;
using web::WebStateImpl;

@interface ShareToDataBuilderTestTabMock : OCMockComplexTypeHelper {
  GURL _visibleURL;
  WebStateImpl* _webState;
}

@property(nonatomic, assign) const GURL& visibleURL;
@property(nonatomic, assign) WebStateImpl* webState;

@end

@implementation ShareToDataBuilderTestTabMock
- (const GURL&)visibleURL {
  return _visibleURL;
}
- (void)setVisibleURL:(const GURL&)visibleURL {
  _visibleURL = visibleURL;
}
- (WebStateImpl*)webState {
  return _webState;
}
- (void)setWebState:(WebStateImpl*)webState {
  _webState = webState;
}
@end

using ShareToDataBuilderTest = PlatformTest;

// Verifies that ShareToData is constructed properly for a given Tab.
TEST_F(ShareToDataBuilderTest, TestSharePageCommandHandling) {
  GURL expected_url("http://www.testurl.net");
  NSString* expected_title = @"title";

  web::TestWebThreadBundle thread_bundle;
  TestChromeBrowserState::Builder test_cbs_builder;
  std::unique_ptr<ios::ChromeBrowserState> chrome_browser_state =
      test_cbs_builder.Build();
  web::WebState::CreateParams params(chrome_browser_state.get());
  std::unique_ptr<web::WebState> webState = web::WebState::Create(params);

  ShareToDataBuilderTestTabMock* tab = [[ShareToDataBuilderTestTabMock alloc]
      initWithRepresentedObject:[OCMockObject niceMockForClass:[Tab class]]];
  [tab setWebState:static_cast<web::WebStateImpl*>(webState.release())];

  tab.visibleURL = expected_url;
  OCMockObject* tab_mock = static_cast<OCMockObject*>(tab);

  ios::ChromeBrowserState* ptr = chrome_browser_state.get();
  [[[tab_mock stub] andReturnValue:OCMOCK_VALUE(ptr)] browserState];
  [[[tab_mock stub] andReturn:expected_title] title];
  [[[tab_mock stub] andReturn:expected_title] originalTitle];

  UIImage* tab_snapshot = ui::test::uiimage_utils::UIImageWithSizeAndSolidColor(
      CGSizeMake(300, 400), [UIColor blueColor]);
  [[[tab_mock stub] andReturn:tab_snapshot] generateSnapshotWithOverlay:NO
                                                       visibleFrameOnly:YES];

  ShareToData* actual_data =
      activity_services::ShareToDataForTab(static_cast<Tab*>(tab));

  ASSERT_TRUE(actual_data);
  EXPECT_EQ(expected_url, actual_data.url);
  EXPECT_NSEQ(expected_title, actual_data.title);
  EXPECT_TRUE(actual_data.isOriginalTitle);
  EXPECT_FALSE(actual_data.isPagePrintable);

  CGSize size = CGSizeMake(40, 40);
  EXPECT_TRUE(ui::test::uiimage_utils::UIImagesAreEqual(
      actual_data.thumbnailGenerator(size),
      ui::test::uiimage_utils::UIImageWithSizeAndSolidColor(
          size, [UIColor blueColor])));
}

// Verifies that |ShareToDataForTab()| returns nil if the Tab is in the process
// of being closed.
TEST_F(ShareToDataBuilderTest, TestReturnsNilWhenClosing) {
  GURL expected_url("http://www.testurl.net");
  NSString* expected_title = @"title";

  // Sets WebState to nil because [tab close] clears the WebState.
  ShareToDataBuilderTestTabMock* tab = [[ShareToDataBuilderTestTabMock alloc]
      initWithRepresentedObject:[OCMockObject niceMockForClass:[Tab class]]];
  tab.webState = nil;
  tab.visibleURL = expected_url;
  OCMockObject* tab_mock = static_cast<OCMockObject*>(tab);
  [[[tab_mock stub] andReturn:expected_title] title];
  [[[tab_mock stub] andReturn:expected_title] originalTitle];

  EXPECT_EQ(nil, activity_services::ShareToDataForTab(static_cast<Tab*>(tab)));
}
