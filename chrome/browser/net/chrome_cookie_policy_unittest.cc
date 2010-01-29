// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_cookie_policy.h"

#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace {

class ChromeCookiePolicyTest : public testing::Test {
 public:
  ChromeCookiePolicyTest()
    : ui_thread_(ChromeThread::UI, &message_loop_) {}

 protected:
  MessageLoop message_loop_;
  ChromeThread ui_thread_;
};

TEST_F(ChromeCookiePolicyTest, DefaultValues) {
  TestingProfile profile;
  ChromeCookiePolicy* cookie_policy = profile.GetCookiePolicy();

  // Check setting of default permissions.
  cookie_policy->set_type(net::CookiePolicy::ALLOW_ALL_COOKIES);
  EXPECT_EQ(net::CookiePolicy::ALLOW_ALL_COOKIES, cookie_policy->type());

  // Check per host permissions returned.
  EXPECT_TRUE(cookie_policy->CanSetCookie(GURL("http://www.example.com"),
                                          GURL("http://www.example.com")));
  cookie_policy->SetPerDomainPermission("example.com",
                                        CONTENT_PERMISSION_TYPE_BLOCK);
  EXPECT_FALSE(cookie_policy->CanSetCookie(GURL("http://www.example.com"),
                                           GURL("http://www.example.com")));
  EXPECT_TRUE(cookie_policy->CanSetCookie(GURL("http://other.com"),
                                          GURL("http://other.com")));

  // Check returning settings for a given resource.
  ChromeCookiePolicy::CookiePolicies policies;
  policies = cookie_policy->GetAllPerDomainPermissions();
  EXPECT_EQ(1U, policies.size());
  EXPECT_EQ("example.com", policies.begin()->first);
  EXPECT_EQ(CONTENT_PERMISSION_TYPE_BLOCK, policies.begin()->second);
}

}  // namespace
