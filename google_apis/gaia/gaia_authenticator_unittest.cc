// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_authenticator.h"

#include <string>

#include "googleurl/src/gurl.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;

namespace gaia {

class GaiaAuthenticatorTest : public testing::Test { };

class GaiaAuthMockForGaiaAuthenticator : public GaiaAuthenticator {
 public:
  GaiaAuthMockForGaiaAuthenticator()
      : GaiaAuthenticator("useragent", "serviceid", "http://gaia_url") {}
  ~GaiaAuthMockForGaiaAuthenticator() {}
 protected:
  bool Post(const GURL& url, const string& post_body,
            unsigned long* response_code, string* response_body) {
    *response_code = net::HTTP_OK;
    response_body->assign("body\n");
    return true;
  }

  int GetBackoffDelaySeconds(
      int current_backoff_delay) {
    // Dummy delay value.
    return 5;
  }
};

TEST(GaiaAuthenticatorTest, TestNewlineAtEndOfAuthTokenRemoved) {
  GaiaAuthMockForGaiaAuthenticator mock_auth;
  MessageLoop message_loop;
  mock_auth.set_message_loop(&message_loop);
  GaiaAuthenticator::AuthResults results;
  EXPECT_TRUE(mock_auth.IssueAuthToken(&results, "sid"));
  EXPECT_EQ(0, results.auth_token.compare("body"));
}

}  // namespace gaia

