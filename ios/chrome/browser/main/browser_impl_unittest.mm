// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/browser_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/browser/web_state_list/web_state_list_delegate.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class TestWebStateListDelegate : public WebStateListDelegate {
 public:
  ~TestWebStateListDelegate() override {}
  void WillAddWebState(web::WebState* web_state) override {}
  void WebStateDetached(web::WebState* web_state) override {}
};

class BrowserImplTest : public PlatformTest {
 protected:
  BrowserImplTest()
      : web_state_list_delegate_(), web_state_list_(&web_state_list_delegate_) {
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();

    tab_model_ = [OCMockObject mockForClass:[TabModel class]];
    OCMStub([tab_model_ webStateList]).andReturn(&web_state_list_);
  }

  web::TestWebThreadBundle thread_bundle_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;

  TestWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  id tab_model_;
};

TEST_F(BrowserImplTest, TestAccessors) {
  BrowserImpl browser(chrome_browser_state_.get(), tab_model_);

  EXPECT_EQ(chrome_browser_state_.get(), browser.GetBrowserState());
  EXPECT_EQ(tab_model_, browser.GetTabModel());
  EXPECT_EQ(&web_state_list_, browser.GetWebStateList());
}

}  // namespace
