// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/web_contents/web_contents_mediator.h"

#include "base/memory/ptr_util.h"
#include "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
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
    mediator_ = [[WebContentsMediator alloc] init];
    SetUpWebStateList();
  }
  ~WebContentsMediatorTest() override { [mediator_ disconnect]; }

 protected:
  void SetUpWebStateList() {
    web_state_list_ = base::MakeUnique<WebStateList>(&web_state_list_delegate_);

    for (int i = 0; i < 2; i++) {
      auto web_state = base::MakeUnique<web::TestWebState>();
      auto navigation_manager = base::MakeUnique<StubNavigationManager>();
      navigation_manager->SetItemCount(0);
      web_state->SetView([[UIView alloc] init]);
      web_state->SetNavigationManager(std::move(navigation_manager));
      web_state_list_->InsertWebState(i, std::move(web_state));
    }
    web_state_list_->ActivateWebStateAt(0);
  }

  web::TestWebState* GetWebStateAt(int position) {
    return static_cast<web::TestWebState*>(
        web_state_list_->GetWebStateAt(position));
  }

  StubNavigationManager* GetNavigationManagerAt(int position) {
    return static_cast<StubNavigationManager*>(
        GetWebStateAt(position)->GetNavigationManager());
  }

  WebContentsMediator* mediator_;
  std::unique_ptr<WebStateList> web_state_list_;
  FakeWebStateListDelegate web_state_list_delegate_;
};

// Tests that webUsage is disabled when mediator is disconnected.
TEST_F(WebContentsMediatorTest, TestDisconnect) {
  mediator_.webStateList = web_state_list_.get();
  web::TestWebState* web_state = GetWebStateAt(0);
  EXPECT_TRUE(web_state->IsWebUsageEnabled());
  [mediator_ disconnect];
  EXPECT_FALSE(web_state->IsWebUsageEnabled());
}

// Tests that both the old and new active web states have WebUsageEnabled
// updated.
TEST_F(WebContentsMediatorTest, TestWebUsageEnabled) {
  mediator_.webStateList = web_state_list_.get();
  web::TestWebState* first_web_state = GetWebStateAt(0);
  web::TestWebState* second_web_state = GetWebStateAt(1);
  first_web_state->SetWebUsageEnabled(true);
  second_web_state->SetWebUsageEnabled(false);
  web_state_list_->ActivateWebStateAt(1);
  EXPECT_FALSE(first_web_state->IsWebUsageEnabled());
  EXPECT_TRUE(second_web_state->IsWebUsageEnabled());
}

// Tests that a URL is loaded if the new active web state has zero navigation
// items.
TEST_F(WebContentsMediatorTest, TestURLHasLoaded) {
  mediator_.webStateList = web_state_list_.get();
  StubNavigationManager* navigation_manager = GetNavigationManagerAt(1);
  navigation_manager->SetItemCount(0);
  web_state_list_->ActivateWebStateAt(1);
  EXPECT_TRUE(navigation_manager->GetHasLoadedUrl());
}

// Tests that a URL is not loaded if the new active web state has some
// navigation items.
TEST_F(WebContentsMediatorTest, TestNoLoadURL) {
  mediator_.webStateList = web_state_list_.get();
  StubNavigationManager* navigation_manager = GetNavigationManagerAt(1);
  navigation_manager->SetItemCount(2);
  web_state_list_->ActivateWebStateAt(1);
  EXPECT_FALSE(navigation_manager->GetHasLoadedUrl());
}

// Tests that the consumer is updated immediately once both consumer and
// webStateList are set. This test sets webStateList first.
TEST_F(WebContentsMediatorTest, TestConsumerViewIsSetWebStateListFirst) {
  StubContentsConsumer* consumer = [[StubContentsConsumer alloc] init];
  mediator_.webStateList = web_state_list_.get();
  web::TestWebState* web_state = GetWebStateAt(0);
  EXPECT_NE(web_state->GetView(), consumer.contentView);
  mediator_.consumer = consumer;
  EXPECT_EQ(web_state->GetView(), consumer.contentView);
}

// Tests that the consumer is updated immediately once both consumer and
// webStateList are set. This test sets consumer first.
TEST_F(WebContentsMediatorTest, TestConsumerViewIsSetConsumerFirst) {
  StubContentsConsumer* consumer = [[StubContentsConsumer alloc] init];
  mediator_.consumer = consumer;
  web::TestWebState* web_state = GetWebStateAt(0);
  EXPECT_NE(web_state->GetView(), consumer.contentView);
  mediator_.webStateList = web_state_list_.get();
  EXPECT_EQ(web_state->GetView(), consumer.contentView);
}

}  // namespace
