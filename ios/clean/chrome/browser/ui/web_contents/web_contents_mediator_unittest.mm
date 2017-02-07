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

class StubNavigationManager : public web::TestNavigationManager {
 public:
  int GetItemCount() const override { return item_count_; }

  void LoadURLWithParams(const NavigationManager::WebLoadParams&) override {
    has_loaded_url_ = true;
  }

  void SetItemCount(int count) { item_count_ = count; }
  bool GetHasLoadedUrl() { return has_loaded_url_; }

 private:
  int item_count_;
  bool has_loaded_url_;
};

class WebContentsMediatorTest : public PlatformTest {
 public:
  WebContentsMediatorTest() {
    auto navigation_manager = base::MakeUnique<StubNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    navigation_manager_->SetItemCount(0);
    web_state_.SetNavigationManager(std::move(navigation_manager));
  }

 protected:
  StubNavigationManager* navigation_manager_;
  web::TestWebState web_state_;
};

TEST_F(WebContentsMediatorTest, TestSetWebUsageEnabled) {
  WebContentsMediator* mediator = [[WebContentsMediator alloc] init];

  mediator.webState = &web_state_;
  // Setting the webState should set webUsageEnabled.
  EXPECT_EQ(true, web_state_.IsWebUsageEnabled());
  // Expect that with zero navigation items, a url will be loaded.
  EXPECT_EQ(true, navigation_manager_->GetHasLoadedUrl());

  mediator.webState = nullptr;
  // The previous webState should now have web usage disabled.
  EXPECT_EQ(false, web_state_.IsWebUsageEnabled());
}

TEST_F(WebContentsMediatorTest, TestNoLoadURL) {
  WebContentsMediator* mediator = [[WebContentsMediator alloc] init];

  navigation_manager_->SetItemCount(2);

  mediator.webState = &web_state_;
  // Expect that with nonzero navigation items, no url will be loaded.
  EXPECT_EQ(false, navigation_manager_->GetHasLoadedUrl());
}

TEST_F(WebContentsMediatorTest, TestSetWebStateFirst) {
  WebContentsMediator* mediator = [[WebContentsMediator alloc] init];
  StubContentsConsumer* consumer = [[StubContentsConsumer alloc] init];

  mediator.webState = &web_state_;
  mediator.consumer = consumer;

  // Setting the consumer after the web state should still have the consumer
  // called.
  EXPECT_EQ(web_state_.GetView(), consumer.contentView);
}

TEST_F(WebContentsMediatorTest, TestSetConsumerFirst) {
  WebContentsMediator* mediator = [[WebContentsMediator alloc] init];
  StubContentsConsumer* consumer = [[StubContentsConsumer alloc] init];

  mediator.consumer = consumer;
  mediator.webState = &web_state_;

  // Setting the web_state after the consumer should trigger a call to the
  // consumer.
  EXPECT_EQ(web_state_.GetView(), consumer.contentView);
}
}
