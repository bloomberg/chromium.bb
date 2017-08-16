// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/web_contents/web_contents_mediator.h"

#include "base/memory/ptr_util.h"
#import "ios/clean/chrome/browser/ui/web_contents/web_contents_consumer.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface StubContentsConsumer : NSObject<WebContentsConsumer>
@property(nonatomic, weak) UIView* contentView;
@end

@implementation StubContentsConsumer
@synthesize contentView = _contentView;

- (void)contentViewDidChange:(UIView*)contentView {
  self.contentView = contentView;
}

@end

namespace {

class WebContentsMediatorTest : public PlatformTest {
 public:
  WebContentsMediatorTest() {
    auto navigation_manager = base::MakeUnique<web::TestNavigationManager>();
    test_web_state_.SetView([[UIView alloc] init]);
    test_web_state_.SetNavigationManager(std::move(navigation_manager));

    auto new_navigation_manager =
        base::MakeUnique<web::TestNavigationManager>();
    new_test_web_state_.SetView([[UIView alloc] init]);
    new_test_web_state_.SetNavigationManager(std::move(new_navigation_manager));

    mediator_ = [[WebContentsMediator alloc] init];
  }

  web::TestNavigationManager* navigation_manager() {
    return static_cast<web::TestNavigationManager*>(
        test_web_state_.GetNavigationManager());
  }

  web::TestNavigationManager* new_navigation_manager() {
    return static_cast<web::TestNavigationManager*>(
        new_test_web_state_.GetNavigationManager());
  }

 protected:
  WebContentsMediator* mediator_;
  web::TestWebState test_web_state_;
  web::TestWebState new_test_web_state_;
};

// Tests that a URL is loaded if the new active web state has zero navigation
// items.
TEST_F(WebContentsMediatorTest, TestURLHasLoaded) {
  mediator_.webState = &test_web_state_;
  mediator_.webState = &new_test_web_state_;
  EXPECT_TRUE(navigation_manager()->LoadURLWithParamsWasCalled());
}

// Tests that a URL is not loaded if the new active web state has some
// navigation items.
TEST_F(WebContentsMediatorTest, TestNoLoadURL) {
  mediator_.webState = &test_web_state_;
  new_navigation_manager()->AddItem(GURL("http://foo.com"),
                                    ui::PAGE_TRANSITION_LINK);
  mediator_.webState = &new_test_web_state_;
  EXPECT_FALSE(new_navigation_manager()->LoadURLWithParamsWasCalled());
}

// Tests that the consumer is updated immediately once both consumer and
// webStateList are set. This test sets webStateList first.
TEST_F(WebContentsMediatorTest, TestConsumerViewIsSetWebStateListFirst) {
  StubContentsConsumer* consumer = [[StubContentsConsumer alloc] init];
  mediator_.webState = &test_web_state_;
  EXPECT_NE(test_web_state_.GetView(), consumer.contentView);
  mediator_.consumer = consumer;
  EXPECT_EQ(test_web_state_.GetView(), consumer.contentView);
}

// Tests that the consumer is updated immediately once both consumer and
// webStateList are set. This test sets consumer first.
TEST_F(WebContentsMediatorTest, TestConsumerViewIsSetConsumerFirst) {
  StubContentsConsumer* consumer = [[StubContentsConsumer alloc] init];
  mediator_.consumer = consumer;
  EXPECT_NE(test_web_state_.GetView(), consumer.contentView);
  mediator_.webState = &test_web_state_;
  EXPECT_EQ(test_web_state_.GetView(), consumer.contentView);
}

}  // namespace
