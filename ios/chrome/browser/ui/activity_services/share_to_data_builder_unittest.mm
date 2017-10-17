// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/share_to_data_builder.h"

#include <memory>

#include "base/macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/ui/activity_services/share_to_data.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/test/ios/ui_image_test_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kExpectedUrl[] = "http://www.testurl.net/";
const char kExpectedTitle[] = "title";
}  // namespace

@interface ShareToDataBuilderTestTabMock : OCMockComplexTypeHelper {
  std::unique_ptr<web::TestWebState> _webState;
}

@property(nonatomic, readonly) web::WebState* webState;

@end

@implementation ShareToDataBuilderTestTabMock

- (web::WebState*)webState {
  return _webState.get();
}

- (instancetype)initWithWebState:(std::unique_ptr<web::TestWebState>)webState {
  self = [super
      initWithRepresentedObject:[OCMockObject niceMockForClass:[Tab class]]];
  if (self) {
    _webState = std::move(webState);
  }
  return self;
}

- (void)close {
  _webState.reset();
}

@end

class ShareToDataBuilderTest : public PlatformTest {
 public:
  ShareToDataBuilderTest() {
    chrome_browser_state_ = TestChromeBrowserState::Builder().Build();

    auto navigation_manager = std::make_unique<web::TestNavigationManager>();
    navigation_manager->AddItem(GURL(kExpectedUrl), ui::PAGE_TRANSITION_TYPED);
    navigation_manager->SetLastCommittedItem(navigation_manager->GetItemAtIndex(
        navigation_manager->GetLastCommittedItemIndex()));
    navigation_manager->GetLastCommittedItem()->SetTitle(
        base::UTF8ToUTF16(kExpectedTitle));

    auto web_state = std::make_unique<web::TestWebState>();
    web_state->SetNavigationManager(std::move(navigation_manager));
    web_state->SetBrowserState(chrome_browser_state_.get());
    web_state->SetVisibleURL(GURL(kExpectedUrl));

    tab_ = [[ShareToDataBuilderTestTabMock alloc]
        initWithWebState:std::move(web_state)];
    OCMockObject* tab_mock = static_cast<OCMockObject*>(tab_);

    ios::ChromeBrowserState* ptr = chrome_browser_state_.get();
    NSString* expected_title = base::SysUTF8ToNSString(kExpectedTitle);
    [[[tab_mock stub] andReturnValue:OCMOCK_VALUE(ptr)] browserState];
    [[[tab_mock stub] andReturn:expected_title] title];

    UIImage* tab_snapshot =
        ui::test::uiimage_utils::UIImageWithSizeAndSolidColor(
            CGSizeMake(300, 400), [UIColor blueColor]);
    [[[tab_mock stub] andReturn:tab_snapshot] generateSnapshotWithOverlay:NO
                                                         visibleFrameOnly:YES];
  }

  void TearDown() override {
    [tab_ close];
    tab_ = nil;
    PlatformTest::TearDown();
  }

  Tab* tab() { return static_cast<Tab*>(tab_); }

  ShareToDataBuilderTestTabMock* tab_mock() { return tab_; }

 private:
  web::TestWebThreadBundle thread_bundle_;
  std::unique_ptr<ios::ChromeBrowserState> chrome_browser_state_;
  ShareToDataBuilderTestTabMock* tab_;

  DISALLOW_COPY_AND_ASSIGN(ShareToDataBuilderTest);
};

// Verifies that ShareToData is constructed properly for a given Tab.
TEST_F(ShareToDataBuilderTest, TestSharePageCommandHandling) {
  ShareToData* actual_data = activity_services::ShareToDataForTab(tab());

  ASSERT_TRUE(actual_data);
  EXPECT_EQ(kExpectedUrl, actual_data.url);
  EXPECT_NSEQ(base::SysUTF8ToNSString(kExpectedTitle), actual_data.title);
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
  [tab_mock() close];

  EXPECT_EQ(nil, activity_services::ShareToDataForTab(tab()));
}
