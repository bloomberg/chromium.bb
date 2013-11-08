// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/http_auth_handler_spdyproxy.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kValidOrigin[] = "https://www.proxy.com/";
const char kValidOrigin2[] = "http://www.proxy2.com/";
const char kValidChallenge[] = "SpdyProxy realm=\"SpdyProxy\", "
    "ps=\"1-2-3-4\"";

}  // namespace

namespace spdyproxy {

using net::ERR_INVALID_RESPONSE;
using net::ERR_UNSUPPORTED_AUTH_SCHEME;
using net::OK;
using net::AuthCredentials;
using net::BoundNetLog;
using net::CompletionCallback;
using net::Error;
using net::HttpAuth;
using net::HttpAuthHandler;
using net::HttpRequestInfo;

TEST(HttpAuthHandlerSpdyProxyTest, GenerateAuthToken) {
  // Verifies that challenge parsing is expected as described in individual
  // cases below.
  static const struct {
    Error err1, err2;
    const char* origin;
    const char* challenge;
    const char* username;
    const char* sid;
    const char* expected_credentials;
  } tests[] = {
      // A well-formed challenge where a sid is provided produces a valid
      // response header echoing the sid and ps token.
      { OK, OK,
        kValidOrigin,
        kValidChallenge,
        "",
        "sid-string",
        "SpdyProxy ps=\"1-2-3-4\", sid=\"sid-string\"",},

      // A non-SSL origin returns ERR_UNSUPPORTED_AUTH_SCHEME.
      { ERR_UNSUPPORTED_AUTH_SCHEME, OK,
        "http://www.proxy.com/", "", "", "", "",},

      // An SSL origin not matching the authorized origin returns
      // ERR_UNSUPPORTED_AUTH_SCHEME.
      { ERR_UNSUPPORTED_AUTH_SCHEME, OK,
        "https://www.unconfigured.com/", "", "", "", "",},

      // Absent ps token yields ERR_INVALID_RESPONSE.
      { ERR_INVALID_RESPONSE, OK,
        kValidOrigin, "SpdyProxy realm=\"SpdyProxy\"", "", "", "",},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    GURL origin(tests[i].origin);
    GURL authorized_origin(kValidOrigin);
    std::vector<GURL> authorized_origins;
    authorized_origins.push_back(authorized_origin);
    HttpAuthHandlerSpdyProxy::Factory factory(authorized_origins);
    scoped_ptr<HttpAuthHandler> spdyproxy;
    EXPECT_EQ(tests[i].err1, factory.CreateAuthHandlerFromString(
        tests[i].challenge, HttpAuth::AUTH_PROXY, origin, BoundNetLog(),
        &spdyproxy));
    if (tests[i].err1 != OK) {
      continue;
    }
    AuthCredentials credentials(ASCIIToUTF16(tests[i].username),
                                ASCIIToUTF16(tests[i].sid));
    HttpRequestInfo request_info;
    std::string auth_token;
    int rv = spdyproxy->GenerateAuthToken(&credentials, &request_info,
                                          CompletionCallback(), &auth_token);
    EXPECT_EQ(tests[i].err2, rv);
    if (tests[i].err2 != OK) {
      continue;
    }
    EXPECT_STREQ(tests[i].expected_credentials, auth_token.c_str());
  }
}

TEST(HttpAuthHandlerSpdyProxyTest, NonProxyAuthTypeFails) {
  // Verifies that an authorization request fails if requested by an ordinary
  // site. (i.e., HttpAuth::AUTH_SERVER)
  GURL origin(kValidOrigin);
  GURL accepted_origin(kValidOrigin);
  GURL accepted_origin2(kValidOrigin2);
  std::vector<GURL> accepted_origins;
  accepted_origins.push_back(accepted_origin);
  accepted_origins.push_back(accepted_origin2);
  HttpAuthHandlerSpdyProxy::Factory factory(accepted_origins);
  scoped_ptr<HttpAuthHandler> spdyproxy;
  EXPECT_EQ(ERR_UNSUPPORTED_AUTH_SCHEME, factory.CreateAuthHandlerFromString(
      kValidChallenge, HttpAuth::AUTH_SERVER, origin,
      BoundNetLog(), &spdyproxy));
}

TEST(HttpAuthHandlerSpdyProxyTest, HandleAnotherChallenge) {
  // Verifies that any repeat challenge is treated as a failure.
  GURL origin(kValidOrigin);
  GURL accepted_origin(kValidOrigin);
  std::vector<GURL> accepted_origins;
  accepted_origins.push_back(accepted_origin);
  HttpAuthHandlerSpdyProxy::Factory factory(accepted_origins);
  scoped_ptr<HttpAuthHandler> spdyproxy;
  EXPECT_EQ(OK, factory.CreateAuthHandlerFromString(
      kValidChallenge, HttpAuth::AUTH_PROXY, origin,
      BoundNetLog(), &spdyproxy));
  std::string challenge(kValidChallenge);
  HttpAuth::ChallengeTokenizer tok(challenge.begin(),
                                   challenge.end());
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_REJECT,
            spdyproxy->HandleAnotherChallenge(&tok));
}

TEST(HttpAuthHandlerSpdyProxyTest, ParseChallenge) {
  // Verifies that various challenge strings are parsed appropriately as
  // described below.
  static const struct {
    const char* challenge;
    int expected_rv;
    const char* expected_ps;
    const char* expected_realm;
  } tests[] = {
    // Absent parameters fails.
    { "SpdyProxy", ERR_INVALID_RESPONSE, "", "", },

    // Empty parameters fails.
    { "SpdyProxy ps=\"\"", ERR_INVALID_RESPONSE, "", "", },

    // Valid challenge parses successfully.
    { kValidChallenge, OK, "1-2-3-4", "SpdyProxy", },
  };
  GURL origin(kValidOrigin);
  GURL accepted_origin(kValidOrigin);
  GURL accepted_origin2(kValidOrigin2);
  std::vector<GURL> accepted_origins;
  accepted_origins.push_back(accepted_origin2);
  accepted_origins.push_back(accepted_origin);
  HttpAuthHandlerSpdyProxy::Factory factory(accepted_origins);
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string challenge = tests[i].challenge;
    scoped_ptr<HttpAuthHandler> spdyproxy;
    int rv = factory.CreateAuthHandlerFromString(
        challenge, HttpAuth::AUTH_PROXY, origin, BoundNetLog(), &spdyproxy);
    EXPECT_EQ(tests[i].expected_rv, rv);
    if (rv == OK) {
      EXPECT_EQ(tests[i].expected_realm, spdyproxy->realm());
      HttpAuthHandlerSpdyProxy* as_spdyproxy =
          static_cast<HttpAuthHandlerSpdyProxy*>(spdyproxy.get());
      EXPECT_EQ(tests[i].expected_ps,
                as_spdyproxy->ps_token_);
    }
  }
}

}  // namespace spdyproxy
