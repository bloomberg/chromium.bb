// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/extensions/api/identity/web_auth_flow.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/net/gaia/oauth2_mint_token_flow.h"
#include "chrome/common/chrome_switches.h"
#include "googleurl/src/gurl.h"

namespace {

class IdentityInterceptor : public OAuth2MintTokenFlow::InterceptorForTests {
 public:
  IdentityInterceptor() : called_(false) { }

  virtual bool DoIntercept(const OAuth2MintTokenFlow* flow,
                           std::string* access_token,
                           GoogleServiceAuthError* error) OVERRIDE {
    *access_token = "auth_token";
    called_ = true;
    return true;
  }

  bool called() const { return called_; }

 private:
  bool called_;
};

class WebAuthFlowInterceptor
    : public extensions::WebAuthFlow::InterceptorForTests {
 public:
  WebAuthFlowInterceptor() : called_(false) { }

  virtual GURL DoIntercept(const GURL& provider_url) OVERRIDE {
    called_ = true;
    return GURL("https://abcd.chromiumapp.org/cb#access_token=tok");
  }

  bool called() const { return called_; }

 private:
  bool called_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(PlatformAppApiTest, Identity) {
  IdentityInterceptor id_interceptor;
  OAuth2MintTokenFlow::SetInterceptorForTests(&id_interceptor);
  WebAuthFlowInterceptor waf_interceptor;
  extensions::WebAuthFlow::SetInterceptorForTests(&waf_interceptor);
  ASSERT_TRUE(RunExtensionTest("identity")) << message_;
  ASSERT_TRUE(id_interceptor.called());
  ASSERT_TRUE(waf_interceptor.called());
};
