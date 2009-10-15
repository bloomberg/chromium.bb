// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/net/gaia_authenticator.h"

#include <string>

#include "chrome/browser/sync/engine/net/http_return.h"
#include "chrome/browser/sync/util/sync_types.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;

namespace browser_sync {

class GaiaAuthenticatorTest : public testing::Test { };

class GaiaAuthMockForGaiaAuthenticator : public GaiaAuthenticator {
 public:
  GaiaAuthMockForGaiaAuthenticator()
      : GaiaAuthenticator("useragent", "serviceid", "http://gaia_url") {}
  ~GaiaAuthMockForGaiaAuthenticator() {}
 protected:
  bool Post(const GURL& url, const string& post_body,
            unsigned long* response_code, string* response_body) {
    *response_code = RC_REQUEST_OK;
    response_body->assign("body\n");
    return true;
  }
};

TEST(GaiaAuthenticatorTest, TestNewlineAtEndOfAuthTokenRemoved) {
  GaiaAuthMockForGaiaAuthenticator mock_auth;
  MessageLoop message_loop;
  mock_auth.set_message_loop(&message_loop);
  GaiaAuthenticator::AuthResults results;
  EXPECT_TRUE(mock_auth.IssueAuthToken(&results, "sid", true));
  EXPECT_EQ(0, results.auth_token.compare("body"));
}

}  // namespace browser_sync
