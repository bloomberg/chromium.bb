// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_usage_enabler_browser_agent.h"

#include "base/macros.h"
#include "base/test/task_environment.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// URL to load in WebStates.
const char kURL[] = "https://chromium.org";
}

class WebUsageEnablerBrowserAgentTest : public PlatformTest {
 public:
  WebUsageEnablerBrowserAgentTest()
      : browser_(std::make_unique<TestBrowser>()),
        web_state_list_(browser_->GetWebStateList()) {
    WebUsageEnablerBrowserAgent::CreateForBrowser(browser_.get());
    enabler_ = WebUsageEnablerBrowserAgent::FromBrowser(browser_.get());
    enabler_->SetWebUsageEnabled(false);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<Browser> browser_;
  WebStateList* web_state_list_;
  WebUsageEnablerBrowserAgent* enabler_;

  std::unique_ptr<web::TestWebState> CreateWebState(const char* url) {
    auto test_web_state = std::make_unique<web::TestWebState>();
    test_web_state->SetCurrentURL(GURL(url));
    test_web_state->SetNavigationManager(
        std::make_unique<web::TestNavigationManager>());
    return test_web_state;
  }

  void AppendNewWebState(const char* url) {
    AppendNewWebState(url, WebStateOpener());
  }

  void AppendNewWebState(const char* url, WebStateOpener opener) {
    web_state_list_->InsertWebState(WebStateList::kInvalidIndex,
                                    CreateWebState(url),
                                    WebStateList::INSERT_NO_FLAGS, opener);
  }

  bool InitialLoadTriggeredForLastWebState() {
    if (web_state_list_->count() <= 0)
      return false;
    web::WebState* last_web_state =
        web_state_list_->GetWebStateAt(web_state_list_->count() - 1);
    web::TestNavigationManager* navigation_manager =
        static_cast<web::TestNavigationManager*>(
            last_web_state->GetNavigationManager());
    return navigation_manager->LoadIfNecessaryWasCalled();
  }

  void VerifyWebUsageEnabled(bool enabled) {
    for (int index = 0; index < web_state_list_->count(); ++index) {
      web::WebState* web_state = web_state_list_->GetWebStateAt(index);
      EXPECT_EQ(web_state->IsWebUsageEnabled(), enabled);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebUsageEnablerBrowserAgentTest);
};

// Tests that calling SetWebUsageEnabled() updates web usage enabled state for
// WebStates already added to the WebStateList as well as those that are added
// afterward.
TEST_F(WebUsageEnablerBrowserAgentTest, EnableWebUsage) {
  // Add a WebState with usage disabled.
  AppendNewWebState(kURL);
  VerifyWebUsageEnabled(false);
  // Enable web usage and add another WebState.  All WebStates in the list
  // should have usage enabled, including the most recently added.
  enabler_->SetWebUsageEnabled(true);
  AppendNewWebState(kURL);
  VerifyWebUsageEnabled(true);
  // Disable web usage and add another WebState.  All WebStates in the list
  // should have usage disabled, including the most recently added.
  enabler_->SetWebUsageEnabled(false);
  AppendNewWebState(kURL);
  VerifyWebUsageEnabled(false);
}

// Tests that TriggersInitialLoad() correctly controls whether the initial load
// of newly added WebStates from being kicked off.
TEST_F(WebUsageEnablerBrowserAgentTest, DisableInitialLoad) {
  enabler_->SetWebUsageEnabled(true);
  // Disable the initial load and verify that the added WebState's
  // LoadIfNecessary() was not called.
  enabler_->SetTriggersInitialLoad(false);
  AppendNewWebState(kURL);
  EXPECT_FALSE(InitialLoadTriggeredForLastWebState());
  // Enable the initial load and verify that the added WebState's
  // LoadIfNecessary() was called.
  enabler_->SetTriggersInitialLoad(true);
  AppendNewWebState(kURL);
  EXPECT_TRUE(InitialLoadTriggeredForLastWebState());
}
