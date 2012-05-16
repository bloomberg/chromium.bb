// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests common functionality used by the Chrome Extensions Cookies API
// implementation.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/values.h"
#include "chrome/browser/extensions/api/cookies/cookies_api_constants.h"
#include "chrome/browser/extensions/api/cookies/cookies_helpers.h"
#include "chrome/test/base/testing_profile.h"
#include "googleurl/src/gurl.h"

namespace extensions {

namespace keys = cookies_api_constants;

namespace {

struct DomainMatchCase {
  const char* filter;
  const char* domain;
  const bool matches;
};

// A test profile that supports linking with another profile for incognito
// support.
class OtrTestingProfile : public TestingProfile {
 public:
  OtrTestingProfile() : linked_profile_(NULL) {}
  virtual Profile* GetOriginalProfile() {
    if (IsOffTheRecord())
      return linked_profile_;
    else
      return this;
  }

  virtual Profile* GetOffTheRecordProfile() {
    if (IsOffTheRecord())
      return this;
    else
      return linked_profile_;
  }

  virtual bool HasOffTheRecordProfile() {
    return (!IsOffTheRecord() && linked_profile_);
  }

  static void LinkProfiles(OtrTestingProfile* profile1,
                           OtrTestingProfile* profile2) {
    profile1->set_linked_profile(profile2);
    profile2->set_linked_profile(profile1);
  }

  void set_linked_profile(OtrTestingProfile* profile) {
    linked_profile_ = profile;
  }

 private:
  OtrTestingProfile* linked_profile_;
};

}  // namespace

class ExtensionCookiesTest : public testing::Test {
};

TEST_F(ExtensionCookiesTest, StoreIdProfileConversion) {
  OtrTestingProfile profile, otrProfile;
  otrProfile.set_incognito(true);
  OtrTestingProfile::LinkProfiles(&profile, &otrProfile);

  EXPECT_EQ(std::string("0"),
            cookies_helpers::GetStoreIdFromProfile(&profile));
  EXPECT_EQ(&profile,
            cookies_helpers::ChooseProfileFromStoreId(
                "0", &profile, true));
  EXPECT_EQ(&profile,
            cookies_helpers::ChooseProfileFromStoreId(
                "0", &profile, false));
  EXPECT_EQ(&otrProfile,
            cookies_helpers::ChooseProfileFromStoreId(
                "1", &profile, true));
  EXPECT_EQ(NULL,
            cookies_helpers::ChooseProfileFromStoreId(
                "1", &profile, false));

  EXPECT_EQ(std::string("1"),
            cookies_helpers::GetStoreIdFromProfile(&otrProfile));
  EXPECT_EQ(NULL,
            cookies_helpers::ChooseProfileFromStoreId(
                "0", &otrProfile, true));
  EXPECT_EQ(NULL,
            cookies_helpers::ChooseProfileFromStoreId(
                "0", &otrProfile, false));
  EXPECT_EQ(&otrProfile,
            cookies_helpers::ChooseProfileFromStoreId(
                "1", &otrProfile, true));
  EXPECT_EQ(&otrProfile,
            cookies_helpers::ChooseProfileFromStoreId(
                "1", &otrProfile, false));
}

TEST_F(ExtensionCookiesTest, ExtensionTypeCreation) {
  std::string string_value;
  bool boolean_value;
  double double_value;
  Value* value;

  net::CookieMonster::CanonicalCookie cookie1(
      GURL(), "ABC", "DEF", "www.foobar.com", "/",
      std::string(), std::string(),
      base::Time(), base::Time(), base::Time(),
      false, false, false, false);
  scoped_ptr<DictionaryValue> cookie_value1(
      cookies_helpers::CreateCookieValue(
          cookie1, "some cookie store"));
  EXPECT_TRUE(cookie_value1->GetString(keys::kNameKey, &string_value));
  EXPECT_EQ("ABC", string_value);
  EXPECT_TRUE(cookie_value1->GetString(keys::kValueKey, &string_value));
  EXPECT_EQ("DEF", string_value);
  EXPECT_TRUE(cookie_value1->GetString(keys::kDomainKey, &string_value));
  EXPECT_EQ("www.foobar.com", string_value);
  EXPECT_TRUE(cookie_value1->GetBoolean(keys::kHostOnlyKey, &boolean_value));
  EXPECT_TRUE(boolean_value);
  EXPECT_TRUE(cookie_value1->GetString(keys::kPathKey, &string_value));
  EXPECT_EQ("/", string_value);
  EXPECT_TRUE(cookie_value1->GetBoolean(keys::kSecureKey, &boolean_value));
  EXPECT_FALSE(boolean_value);
  EXPECT_TRUE(cookie_value1->GetBoolean(keys::kHttpOnlyKey, &boolean_value));
  EXPECT_FALSE(boolean_value);
  EXPECT_TRUE(cookie_value1->GetBoolean(keys::kSessionKey, &boolean_value));
  EXPECT_TRUE(boolean_value);
  EXPECT_FALSE(
      cookie_value1->GetDouble(keys::kExpirationDateKey, &double_value));
  EXPECT_TRUE(cookie_value1->GetString(keys::kStoreIdKey, &string_value));
  EXPECT_EQ("some cookie store", string_value);

  net::CookieMonster::CanonicalCookie cookie2(
      GURL(), "ABC", "DEF", ".foobar.com", "/", std::string(), std::string(),
      base::Time(), base::Time::FromDoubleT(10000), base::Time(),
      false, false, true, true);
  scoped_ptr<DictionaryValue> cookie_value2(
      cookies_helpers::CreateCookieValue(
          cookie2, "some cookie store"));
  EXPECT_TRUE(cookie_value2->GetBoolean(keys::kHostOnlyKey, &boolean_value));
  EXPECT_FALSE(boolean_value);
  EXPECT_TRUE(cookie_value2->GetBoolean(keys::kSessionKey, &boolean_value));
  EXPECT_FALSE(boolean_value);
  EXPECT_TRUE(
      cookie_value2->GetDouble(keys::kExpirationDateKey, &double_value));
  EXPECT_EQ(10000, double_value);

  TestingProfile profile;
  ListValue* tab_ids = new ListValue();
  scoped_ptr<DictionaryValue> cookie_store_value(
      cookies_helpers::CreateCookieStoreValue(&profile, tab_ids));
  EXPECT_TRUE(cookie_store_value->GetString(keys::kIdKey, &string_value));
  EXPECT_EQ("0", string_value);
  EXPECT_TRUE(cookie_store_value->Get(keys::kTabIdsKey, &value));
  EXPECT_EQ(tab_ids, value);
}

TEST_F(ExtensionCookiesTest, GetURLFromCanonicalCookie) {
  net::CookieMonster::CanonicalCookie cookie1(
      GURL(), "ABC", "DEF", "www.foobar.com", "/",
      std::string(), std::string(),
      base::Time(), base::Time(), base::Time(),
      false, false, false, false);
  EXPECT_EQ("http://www.foobar.com/",
            cookies_helpers::GetURLFromCanonicalCookie(
                cookie1).spec());

  net::CookieMonster::CanonicalCookie cookie2(
      GURL(), "ABC", "DEF", ".helloworld.com", "/",
      std::string(), std::string(),
      base::Time(), base::Time(), base::Time(),
      true, false, false, false);
  EXPECT_EQ("https://helloworld.com/",
            cookies_helpers::GetURLFromCanonicalCookie(
                cookie2).spec());
}

TEST_F(ExtensionCookiesTest, EmptyDictionary) {
  scoped_ptr<DictionaryValue> details(new DictionaryValue());
  cookies_helpers::MatchFilter filter(details.get());
  std::string domain;
  net::CookieMonster::CanonicalCookie cookie;

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

  scoped_ptr<DictionaryValue> details(new DictionaryValue());
  for (size_t i = 0; i < arraysize(tests); ++i) {
    details->SetString(keys::kDomainKey, std::string(tests[i].filter));
    cookies_helpers::MatchFilter filter(details.get());
    net::CookieMonster::CanonicalCookie cookie(GURL(), "", "", tests[i].domain,
                                               "", "", "", base::Time(),
                                               base::Time(), base::Time(),
                                               false, false, false, false);
    EXPECT_EQ(tests[i].matches, filter.MatchesCookie(cookie));
  }
}

TEST_F(ExtensionCookiesTest, DecodeUTF8WithErrorHandling) {
  net::CookieMonster::CanonicalCookie cookie(GURL(), "",
                                             "011Q255bNX_1!yd\203e+",
                                             "test.com",
                                             "/path\203", "", "", base::Time(),
                                             base::Time(), base::Time(),
                                             false, false, false, false);
  scoped_ptr<DictionaryValue> cookie_value(
      cookies_helpers::CreateCookieValue(
          cookie, "some cookie store"));
  std::string string_value;
  EXPECT_TRUE(cookie_value->GetString(keys::kValueKey, &string_value));
  EXPECT_EQ(std::string("011Q255bNX_1!yd\xEF\xBF\xBD" "e+"), string_value);
  EXPECT_TRUE(cookie_value->GetString(keys::kPathKey, &string_value));
  EXPECT_EQ(std::string(""), string_value);
}

}  // namespace extensions
