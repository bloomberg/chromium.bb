// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "net/base/auth.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kHttpUrl[] = "http://example.com/foo/bar";
const char kBasicAuthScheme[] = "Basic";
const char kFooRealm[] = "Foo";
const char kInsecureProxy[] = "Your connection to this site is not secure";

enum TargetType { PROXY, SERVER };

const struct TestCase {
  const char* const request_url;
  struct {
    TargetType target_type;
    const char* const scheme;
    const char* const realm;
    const char* const challenger;
  } auth_info;
  struct {
    const char* const authority;
    const char* const explanation;
    const char* const signon_realm;
  } expected;
} kTestCases[]{
    // Insecure proxy
    {kHttpUrl,
     {PROXY, kBasicAuthScheme, kFooRealm, "http://example.com"},
     {"The proxy http://example.com requires a username and password.",
      kInsecureProxy, "example.com:80/Foo"}},

    // Insecure proxy on non-standard port
    {kHttpUrl,
     {PROXY, kBasicAuthScheme, kFooRealm, "http://example.com:8009"},
     {"The proxy http://example.com:8009 requires a username and password.",
      kInsecureProxy, "example.com:8009/Foo"}},

    // Secure proxy
    {kHttpUrl,
     {PROXY, kBasicAuthScheme, kFooRealm, "https://example.com"},
     {"The proxy https://example.com requires a username and password.", "",
      "example.com:443/Foo"}},

    // Secure proxy on non-standard port
    {kHttpUrl,
     {PROXY, kBasicAuthScheme, kFooRealm, "https://example.com:446"},
     {"The proxy https://example.com:446 requires a username and password.", "",
      "example.com:446/Foo"}},

    // localhost
    {kHttpUrl,
     {PROXY, kBasicAuthScheme, kFooRealm, "http://localhost:7323"},
     {"The proxy http://localhost:7323 requires a username and password.", "",
      "localhost:7323/Foo"}},

    // Secure server
    {"https://www.nowhere.org/dir/index.html",
     {SERVER, kBasicAuthScheme, kFooRealm, nullptr},
     {"https://www.nowhere.org requires a username and password.", "",
      "https://www.nowhere.org/Foo"}},

    // URL uses default port.
    {"https://www.nowhere.org:443/dir/index.html",
     {SERVER, kBasicAuthScheme, kFooRealm, nullptr},
     {"https://www.nowhere.org requires a username and password.", "",
      "https://www.nowhere.org/Foo"}},

    // URL uses non-default port.
    {"https://www.nowhere.org:8443/dir/index.html",
     {SERVER, kBasicAuthScheme, kFooRealm, nullptr},
     {"https://www.nowhere.org:8443 requires a username and password.", "",
      "https://www.nowhere.org:8443/Foo"}},

    // URL has no trailing slash.
    {"https://www.nowhere.org",
     {SERVER, kBasicAuthScheme, kFooRealm, nullptr},
     {"https://www.nowhere.org requires a username and password.", "",
      "https://www.nowhere.org/Foo"}},

    // username:password
    {"https://foo:bar@www.nowhere.org/dir/index.html",
     {SERVER, kBasicAuthScheme, kFooRealm, nullptr},
     {"https://www.nowhere.org requires a username and password.", "",
      "https://www.nowhere.org/Foo"}},

    // query
    {"https://www.nowhere.org/dir/index.html?id=965362",
     {SERVER, kBasicAuthScheme, kFooRealm, nullptr},
     {"https://www.nowhere.org requires a username and password.", "",
      "https://www.nowhere.org/Foo"}},

    // reference
    {"https://www.nowhere.org/dir/index.html#toc",
     {SERVER, kBasicAuthScheme, kFooRealm, nullptr},
     {"https://www.nowhere.org requires a username and password.", "",
      "https://www.nowhere.org/Foo"}},
};

}  // namespace

TEST(LoginHandlerTest, DialogStringsAndRealm) {
  scoped_refptr<net::AuthChallengeInfo> auth_info = new net::AuthChallengeInfo;
  for (const auto& test_case : kTestCases) {
    GURL request_url(test_case.request_url);
    auth_info->is_proxy = test_case.auth_info.target_type == PROXY;
    auth_info->scheme = test_case.auth_info.scheme;
    auth_info->realm = test_case.auth_info.realm;
    auth_info->challenger = url::Origin(
        test_case.auth_info.challenger ? GURL(test_case.auth_info.challenger)
                                       : request_url);

    SCOPED_TRACE(::testing::Message()
                 << "request_url:" << test_case.request_url
                 << " auth_info: { is_proxy:" << auth_info->is_proxy
                 << " scheme:'" << auth_info->scheme << "' realm:'"
                 << auth_info->realm << "' challenger:'"
                 << auth_info->challenger.Serialize() << "' }");
    base::string16 authority;
    base::string16 explanation;

    LoginHandler::GetDialogStrings(request_url, *auth_info, &authority,
                                   &explanation);
    EXPECT_STREQ(test_case.expected.authority,
                 base::UTF16ToASCII(authority).c_str());
    EXPECT_STREQ(test_case.expected.explanation,
                 base::UTF16ToASCII(explanation).c_str());

    EXPECT_STREQ(test_case.expected.signon_realm,
                 LoginHandler::GetSignonRealm(request_url, *auth_info).c_str());
  }
}
