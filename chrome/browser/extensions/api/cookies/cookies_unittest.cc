// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests common functionality used by the Chrome Extensions Cookies API
// implementation.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/values.h"
#include "chrome/browser/extensions/api/cookies/cookies_api_constants.h"
#include "chrome/browser/extensions/api/cookies/cookies_helpers.h"
#include "chrome/common/extensions/api/cookies.h"
#include "chrome/test/base/testing_profile.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "url/gurl.h"

using extensions::api::cookies::Cookie;
using extensions::api::cookies::CookieStore;

namespace GetAll = extensions::api::cookies::GetAll;

namespace extensions {

namespace keys = cookies_api_constants;

namespace {

struct DomainMatchCase {
  const char* filter;
  const char* domain;
  const bool matches;
};

}  // namespace

class ExtensionCookiesTest : public testing::Test {
};

TEST_F(ExtensionCookiesTest, StoreIdProfileConversion) {
  TestingProfile::Builder profile_builder;
  scoped_ptr<TestingProfile> profile = profile_builder.Build();
  // Trigger early creation of off-the-record profile.
  EXPECT_TRUE(profile->GetOffTheRecordProfile());

  EXPECT_EQ(std::string("0"),
            cookies_helpers::GetStoreIdFromProfile(profile.get()));
  EXPECT_EQ(profile.get(),
            cookies_helpers::ChooseProfileFromStoreId(
                "0", profile.get(), true));
  EXPECT_EQ(profile.get(),
            cookies_helpers::ChooseProfileFromStoreId(
                "0", profile.get(), false));
  EXPECT_EQ(profile->GetOffTheRecordProfile(),
            cookies_helpers::ChooseProfileFromStoreId(
                "1", profile.get(), true));
  EXPECT_EQ(NULL,
            cookies_helpers::ChooseProfileFromStoreId(
                "1", profile.get(), false));

  EXPECT_EQ(std::string("1"),
            cookies_helpers::GetStoreIdFromProfile(
                profile->GetOffTheRecordProfile()));
  EXPECT_EQ(NULL,
            cookies_helpers::ChooseProfileFromStoreId(
                "0", profile->GetOffTheRecordProfile(), true));
  EXPECT_EQ(NULL,
            cookies_helpers::ChooseProfileFromStoreId(
                "0", profile->GetOffTheRecordProfile(), false));
  EXPECT_EQ(profile->GetOffTheRecordProfile(),
            cookies_helpers::ChooseProfileFromStoreId(
                "1", profile->GetOffTheRecordProfile(), true));
  EXPECT_EQ(profile->GetOffTheRecordProfile(),
            cookies_helpers::ChooseProfileFromStoreId(
                "1", profile->GetOffTheRecordProfile(), false));
}

TEST_F(ExtensionCookiesTest, ExtensionTypeCreation) {
  net::CanonicalCookie canonical_cookie1(
      GURL(), "ABC", "DEF", "www.foobar.com", "/",
      base::Time(), base::Time(), base::Time(),
      false, false, net::COOKIE_PRIORITY_DEFAULT);
  scoped_ptr<Cookie> cookie1(
      cookies_helpers::CreateCookie(
          canonical_cookie1, "some cookie store"));
  EXPECT_EQ("ABC", cookie1->name);
  EXPECT_EQ("DEF", cookie1->value);
  EXPECT_EQ("www.foobar.com", cookie1->domain);
  EXPECT_TRUE(cookie1->host_only);
  EXPECT_EQ("/", cookie1->path);
  EXPECT_FALSE(cookie1->secure);
  EXPECT_FALSE(cookie1->http_only);
  EXPECT_TRUE(cookie1->session);
  EXPECT_FALSE(cookie1->expiration_date.get());
  EXPECT_EQ("some cookie store", cookie1->store_id);

  net::CanonicalCookie canonical_cookie2(
      GURL(), "ABC", "DEF", ".foobar.com", "/",
      base::Time(), base::Time::FromDoubleT(10000), base::Time(),
      false, false, net::COOKIE_PRIORITY_DEFAULT);
  scoped_ptr<Cookie> cookie2(
      cookies_helpers::CreateCookie(
          canonical_cookie2, "some cookie store"));
  EXPECT_FALSE(cookie2->host_only);
  EXPECT_FALSE(cookie2->session);
  ASSERT_TRUE(cookie2->expiration_date.get());
  EXPECT_EQ(10000, *cookie2->expiration_date);

  TestingProfile profile;
  base::ListValue* tab_ids_list = new base::ListValue();
  std::vector<int> tab_ids;
  scoped_ptr<CookieStore> cookie_store(
      cookies_helpers::CreateCookieStore(&profile, tab_ids_list));
  EXPECT_EQ("0", cookie_store->id);
  EXPECT_EQ(tab_ids, cookie_store->tab_ids);
}

TEST_F(ExtensionCookiesTest, GetURLFromCanonicalCookie) {
  net::CanonicalCookie cookie1(
      GURL(), "ABC", "DEF", "www.foobar.com", "/", base::Time(), base::Time(),
      base::Time(), false, false, net::COOKIE_PRIORITY_DEFAULT);
  EXPECT_EQ("http://www.foobar.com/",
            cookies_helpers::GetURLFromCanonicalCookie(
                cookie1).spec());

  net::CanonicalCookie cookie2(
      GURL(), "ABC", "DEF", ".helloworld.com", "/", base::Time(), base::Time(),
      base::Time(), true, false, net::COOKIE_PRIORITY_DEFAULT);
  EXPECT_EQ("https://helloworld.com/",
            cookies_helpers::GetURLFromCanonicalCookie(
                cookie2).spec());
}

TEST_F(ExtensionCookiesTest, EmptyDictionary) {
  base::DictionaryValue dict;
  GetAll::Params::Details details;
  bool rv = GetAll::Params::Details::Populate(dict, &details);
  ASSERT_TRUE(rv);
  cookies_helpers::MatchFilter filter(&details);
  net::CanonicalCookie cookie;
  EXPECT_TRUE(filter.MatchesCookie(cookie));
}

TEST_F(ExtensionCookiesTest, DomainMatching) {
  const DomainMatchCase tests[] = {
    { "bar.com", "bar.com", true },
    { ".bar.com", "bar.com", true },
    { "bar.com", "foo.bar.com", true },
    { "bar.com", "bar.foo.com", false },
    { ".bar.com", ".foo.bar.com", true },
    { ".bar.com", "baz.foo.bar.com", true },
    { "foo.bar.com", ".bar.com", false }
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    // Build up the Params struct.
    base::ListValue args;
    base::DictionaryValue* dict = new base::DictionaryValue();
    dict->SetString(keys::kDomainKey, std::string(tests[i].filter));
    args.Set(0, dict);
    scoped_ptr<GetAll::Params> params(GetAll::Params::Create(args));

    cookies_helpers::MatchFilter filter(&params->details);
    net::CanonicalCookie cookie(GURL(),
                                std::string(),
                                std::string(),
                                tests[i].domain,
                                std::string(),
                                base::Time(),
                                base::Time(),
                                base::Time(),
                                false,
                                false,
                                net::COOKIE_PRIORITY_DEFAULT);
    EXPECT_EQ(tests[i].matches, filter.MatchesCookie(cookie));
  }
}

TEST_F(ExtensionCookiesTest, DecodeUTF8WithErrorHandling) {
  net::CanonicalCookie canonical_cookie(GURL(),
                                        std::string(),
                                        "011Q255bNX_1!yd\203e+",
                                        "test.com",
                                        "/path\203",
                                        base::Time(),
                                        base::Time(),
                                        base::Time(),
                                        false,
                                        false,
                                        net::COOKIE_PRIORITY_DEFAULT);
  scoped_ptr<Cookie> cookie(
      cookies_helpers::CreateCookie(
          canonical_cookie, "some cookie store"));
  EXPECT_EQ(std::string("011Q255bNX_1!yd\xEF\xBF\xBD" "e+"), cookie->value);
  EXPECT_EQ(std::string(), cookie->path);
}

}  // namespace extensions
