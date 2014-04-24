// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_cookie_access_policy.h"

#include "base/run_loop.h"
#include "net/base/request_priority.h"
#include "net/cookies/canonical_cookie.h"
#include "net/url_request/url_request_test_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class ResourceContext;
}

namespace net {
class CookieOptions;
}

class GURL;

using android_webview::AwCookieAccessPolicy;
using net::CookieList;
using net::TestDelegate;
using net::TestJobInterceptor;
using net::TestNetworkDelegate;
using net::TestURLRequestContext;
using net::TestURLRequest;
using testing::Test;

class AwCookieAccessPolicyTest : public Test {
 public:
  static const GURL kUrlFirstParty;
  static const GURL kUrlThirdParty;

  AwCookieAccessPolicyTest()
      : loop_(),
        context_(),
        url_request_delegate_(),
        network_delegate_(),
        first_party_request_() {}

  virtual void SetUp() {
    context_.set_network_delegate(&network_delegate_);
    first_party_request_.reset(new TestURLRequest(kUrlFirstParty,
                                                  net::DEFAULT_PRIORITY,
                                                  &url_request_delegate_,
                                                  &context_));
    first_party_request_->set_method("GET");

    third_party_request_.reset(new TestURLRequest(kUrlThirdParty,
                                                  net::DEFAULT_PRIORITY,
                                                  &url_request_delegate_,
                                                  &context_));
    third_party_request_->set_method("GET");
    third_party_request_->set_first_party_for_cookies(kUrlFirstParty);
  }

  AwCookieAccessPolicy* policy() {
    return AwCookieAccessPolicy::GetInstance();
  }

  bool GetGlobalAllowAccess() {
    return policy()->GetGlobalAllowAccess();
  }

  void SetGlobalAllowAccess(bool allow) {
    policy()->SetGlobalAllowAccess(allow);
  }

  bool GetThirdPartyAllowAccess() {
    return policy()->GetThirdPartyAllowAccess();
  }

  void SetThirdPartyAllowAccess(bool allow) {
    policy()->SetThirdPartyAllowAccess(allow);
  }

  bool OnCanGetCookies(const net::URLRequest& request) {
    return policy()->OnCanGetCookies(request, CookieList());
  }

  bool OnCanSetCookie(const net::URLRequest& request) {
    return policy()->OnCanSetCookie(request, "", NULL);
  }

  bool AllowGetCookie(const GURL& url, const GURL& first_party) {
    return policy()->AllowGetCookie(url, first_party, CookieList(), NULL, 0, 0);
  }

  bool AllowSetCookie(const GURL& url, const GURL& first_party) {
    return policy()->AllowSetCookie(url, first_party, "", NULL, 0, 0, NULL);
  }

  void expectFirstPartyAccess(bool expectedResult) {
    EXPECT_EQ(expectedResult, AllowSetCookie(kUrlFirstParty, kUrlFirstParty));
    EXPECT_EQ(expectedResult, AllowGetCookie(kUrlFirstParty, kUrlFirstParty));
    EXPECT_EQ(expectedResult, OnCanGetCookies(*first_party_request_));
    EXPECT_EQ(expectedResult, OnCanSetCookie(*first_party_request_));
  }

  void expectThirdPartyAccess(bool expectedResult) {
    EXPECT_EQ(expectedResult, AllowSetCookie(kUrlFirstParty, kUrlThirdParty));
    EXPECT_EQ(expectedResult, AllowGetCookie(kUrlFirstParty, kUrlThirdParty));
    EXPECT_EQ(expectedResult, OnCanGetCookies(*third_party_request_));
    EXPECT_EQ(expectedResult, OnCanSetCookie(*third_party_request_));
  }

 protected:
  base::MessageLoopForIO loop_;
  TestURLRequestContext context_;
  TestDelegate url_request_delegate_;
  TestNetworkDelegate network_delegate_;
  scoped_ptr<TestURLRequest> first_party_request_;
  scoped_ptr<TestURLRequest> third_party_request_;
};

const GURL AwCookieAccessPolicyTest::kUrlFirstParty =
    GURL("http://first.example");
const GURL AwCookieAccessPolicyTest::kUrlThirdParty =
    GURL("http://third.example");

TEST_F(AwCookieAccessPolicyTest, BlockAllCookies) {
  SetGlobalAllowAccess(false);
  SetThirdPartyAllowAccess(false);
  expectFirstPartyAccess(false);
  expectThirdPartyAccess(false);
}

TEST_F(AwCookieAccessPolicyTest, BlockAllCookiesWithThirdPartySet) {
  SetGlobalAllowAccess(false);
  SetThirdPartyAllowAccess(true);
  expectFirstPartyAccess(false);
  expectThirdPartyAccess(false);
}

TEST_F(AwCookieAccessPolicyTest, FirstPartyCookiesOnly) {
  SetGlobalAllowAccess(true);
  SetThirdPartyAllowAccess(false);
  expectFirstPartyAccess(true);
  expectThirdPartyAccess(false);
}

TEST_F(AwCookieAccessPolicyTest, AllowAllCookies) {
  SetGlobalAllowAccess(true);
  SetThirdPartyAllowAccess(true);
  expectFirstPartyAccess(true);
  expectThirdPartyAccess(true);
}
