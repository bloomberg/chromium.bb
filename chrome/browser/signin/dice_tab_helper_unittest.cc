// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_tab_helper.h"

#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests DiceTabHelper intialization.
TEST(DiceTabHelperTest, Initialization) {
  content::TestBrowserThreadBundle thread_bundle;
  TestingProfile profile;
  content::TestWebContentsFactory factory;
  content::WebContents* web_contents = factory.CreateWebContents(&profile);
  DiceTabHelper::CreateForWebContents(web_contents);
  DiceTabHelper* dice_tab_helper = DiceTabHelper::FromWebContents(web_contents);

  // Check default state.
  EXPECT_EQ(signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
            dice_tab_helper->signin_access_point());
  EXPECT_EQ(signin_metrics::Reason::REASON_UNKNOWN_REASON,
            dice_tab_helper->signin_reason());
  EXPECT_TRUE(dice_tab_helper->should_start_sync_after_web_signin());

  // Initialize the signin flow.
  signin_metrics::AccessPoint access_point =
      signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE;
  signin_metrics::Reason reason =
      signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT;
  dice_tab_helper->InitializeSigninFlow(access_point, reason);
  EXPECT_EQ(access_point, dice_tab_helper->signin_access_point());
  EXPECT_EQ(reason, dice_tab_helper->signin_reason());
}
