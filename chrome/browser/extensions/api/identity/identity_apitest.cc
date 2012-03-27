// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/net/gaia/oauth2_mint_token_flow.h"
#include "chrome/common/chrome_switches.h"

namespace {

class IdentityInterceptor : public OAuth2MintTokenFlow::InterceptorForTests {
 public:
  virtual bool DoIntercept(const OAuth2MintTokenFlow* flow,
                           std::string* access_token,
                           GoogleServiceAuthError* error) OVERRIDE {
    *access_token = "auth_token";
    get_auth_token_called_ = true;
    return true;
  }

  bool get_auth_token_called() const { return get_auth_token_called_; }

 private:
  bool get_auth_token_called_;
};

}  // namespace

class ExperimentalApiTest : public PlatformAppApiTest {
};

IN_PROC_BROWSER_TEST_F(ExperimentalApiTest, IdentityVerifyPermissions) {
  VerifyPermissions(test_data_dir_.AppendASCII("identity"));
}

IN_PROC_BROWSER_TEST_F(ExperimentalApiTest, Identity) {
  IdentityInterceptor interceptor;
  OAuth2MintTokenFlow::SetInterceptorForTests(&interceptor);
  ASSERT_TRUE(RunExtensionTest("identity")) << message_;
  ASSERT_TRUE(interceptor.get_auth_token_called());
};
