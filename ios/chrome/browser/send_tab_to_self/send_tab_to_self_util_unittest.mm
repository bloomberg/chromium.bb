// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/send_tab_to_self/send_tab_to_self_util.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "components/send_tab_to_self/features.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace send_tab_to_self {

class SendTabToSelfUtilTest : public PlatformTest {
 public:
  SendTabToSelfUtilTest() {
    TestChromeBrowserState::Builder builder;
    browser_state_ = builder.Build();
  }
  ~SendTabToSelfUtilTest() override {}

  ios::ChromeBrowserState* browser_state() { return browser_state_.get(); }
  ios::ChromeBrowserState* OffTheRecordChromeBrowserState() {
    return browser_state_->GetOffTheRecordChromeBrowserState();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;

 private:
  std::unique_ptr<ios::ChromeBrowserState> browser_state_;
};

TEST_F(SendTabToSelfUtilTest, AreFlagsEnabled) {
  scoped_task_environment_.RunUntilIdle();
  scoped_feature_list_.InitWithFeatures(
      {switches::kSyncSendTabToSelf, kSendTabToSelfShowSendingUI}, {});

  EXPECT_TRUE(IsSendingEnabled());
  EXPECT_TRUE(IsReceivingEnabled());
}

TEST_F(SendTabToSelfUtilTest, AreFlagsDisabled) {
  scoped_task_environment_.RunUntilIdle();
  scoped_feature_list_.InitWithFeatures(
      {}, {switches::kSyncSendTabToSelf, kSendTabToSelfShowSendingUI});

  EXPECT_FALSE(IsSendingEnabled());
  EXPECT_FALSE(IsReceivingEnabled());
}

TEST_F(SendTabToSelfUtilTest, IsReceivingEnabled) {
  scoped_task_environment_.RunUntilIdle();
  scoped_feature_list_.InitWithFeatures({switches::kSyncSendTabToSelf},
                                        {kSendTabToSelfShowSendingUI});

  EXPECT_FALSE(IsSendingEnabled());
  EXPECT_TRUE(IsReceivingEnabled());
}

TEST_F(SendTabToSelfUtilTest, IsOnlySendingEnabled) {
  scoped_task_environment_.RunUntilIdle();
  scoped_feature_list_.InitWithFeatures({kSendTabToSelfShowSendingUI},
                                        {switches::kSyncSendTabToSelf});

  EXPECT_FALSE(IsSendingEnabled());
  EXPECT_FALSE(IsReceivingEnabled());
}

TEST_F(SendTabToSelfUtilTest, NotHTTPOrHTTPS) {
  GURL url = GURL("192.168.0.0");
  EXPECT_FALSE(IsContentRequirementsMet(url, browser_state()));
}

TEST_F(SendTabToSelfUtilTest, WebUIPage) {
  GURL url = GURL("chrome://flags");
  EXPECT_FALSE(IsContentRequirementsMet(url, browser_state()));
}

TEST_F(SendTabToSelfUtilTest, IncognitoMode) {
  GURL url = GURL("https://www.google.com");
  EXPECT_FALSE(IsContentRequirementsMet(url, OffTheRecordChromeBrowserState()));
}

TEST_F(SendTabToSelfUtilTest, ValidUrl) {
  GURL url = GURL("https://www.google.com");
  EXPECT_TRUE(IsContentRequirementsMet(url, browser_state()));
}

// TODO(crbug.com/961897) Add test for CreateNewEntry.

}  // namespace send_tab_to_self
