// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/test/webdriver/cookie.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webdriver {

namespace {

const int kArbitraryDateInSecondsSinceEpoch = 996871631;
const double kArbitraryDateInSecondsSinceEpochF =
    static_cast<double>(kArbitraryDateInSecondsSinceEpoch);
const char* kArbitraryDateString = "03 Aug 2001 20:47:11 GMT";
const base::Time kArbitraryDate =
    base::Time::FromDoubleT(kArbitraryDateInSecondsSinceEpoch);

void ExpectStringEq(const scoped_ptr<DictionaryValue>& dict,
                    const std::string& key,
                    const std::string& expected_value) {
  std::string value;
  EXPECT_TRUE(dict->GetString(key, &value))
      << key << " not found or not a string";
  EXPECT_EQ(expected_value, value);
}


void ExpectBooleanEq(const scoped_ptr<DictionaryValue>& dict,
                     const std::string& key,
                     bool expected_value) {
  bool value;
  EXPECT_TRUE(dict->GetBoolean(key, &value))
      << key << " not found or not a boolean";
  EXPECT_EQ(expected_value, value);
}

void ExpectDoubleEq(const scoped_ptr<DictionaryValue>& dict,
                    const std::string& key,
                    double expected_value) {
  double value;
  EXPECT_TRUE(dict->GetDouble(key, &value))
      << key << " not found or not a double";
  EXPECT_EQ(expected_value, value);
}

}

TEST(CookieTest, ParsingJustNameAndValue) {
  Cookie cookie("name=value");
  EXPECT_EQ("name", cookie.name());
  EXPECT_EQ("value", cookie.value());
  EXPECT_TRUE(cookie.path().empty());
  EXPECT_TRUE(cookie.domain().empty());
  EXPECT_TRUE(cookie.expiration().is_null());
  EXPECT_FALSE(cookie.secure());
  EXPECT_FALSE(cookie.http_only());
}

TEST(CookieTest, EmptyStringsAreInvalidCookies) {
  EXPECT_FALSE(Cookie("").valid());
}

TEST(CookieTest, ParsingCookieWhoseNameIsATokenKeyword_path) {
  Cookie cookie("path=foo; path=/bar;");
  EXPECT_TRUE(cookie.valid());
  EXPECT_EQ("path", cookie.name());
  EXPECT_EQ("foo", cookie.value());
  EXPECT_EQ("/bar", cookie.path());
}

TEST(CookieTest, ParsingCookieWhoseNameIsATokenKeyword_domain) {
  Cookie cookie("domain=chromium.org; domain=google.com");
  EXPECT_TRUE(cookie.valid());
  EXPECT_EQ("domain", cookie.name());
  EXPECT_EQ("chromium.org", cookie.value());
  EXPECT_EQ("google.com", cookie.domain());
}

TEST(CookieTest, TimeToDateString) {
  EXPECT_EQ("01 Jan 1970 00:00:00 GMT",
            Cookie::ToDateString(base::Time::UnixEpoch()));

  EXPECT_EQ(std::string(kArbitraryDateString),
            Cookie::ToDateString(kArbitraryDate));
}

TEST(CookieTest, CreatingCookieFromAString_withExpiration) {
  std::string cookie_string = base::StringPrintf(
      "foo=bar; path=/; domain=chromium.org; expires=%s;",
      kArbitraryDateString);

  Cookie cookie(cookie_string);
  EXPECT_TRUE(cookie.valid());
  EXPECT_EQ("foo", cookie.name());
  EXPECT_EQ("bar", cookie.value());
  EXPECT_EQ("/", cookie.path());
  EXPECT_EQ("chromium.org", cookie.domain());
  EXPECT_FALSE(cookie.secure());
  EXPECT_FALSE(cookie.http_only());
  EXPECT_FALSE(cookie.expiration().is_null());
  EXPECT_EQ(kArbitraryDate, cookie.expiration());

  EXPECT_EQ(cookie_string, cookie.ToString());

  scoped_ptr<DictionaryValue> dict(cookie.ToDictionary());
  EXPECT_EQ(7u, dict->size());
  ExpectStringEq(dict, "name", "foo");
  ExpectStringEq(dict, "value", "bar");
  ExpectStringEq(dict, "path", "/");
  ExpectStringEq(dict, "domain", "chromium.org");
  ExpectBooleanEq(dict, "secure", false);
  ExpectBooleanEq(dict, "http_only", false);
  ExpectDoubleEq(dict, "expiry", kArbitraryDateInSecondsSinceEpochF);
}

TEST(CookieTest, CreatingCookieFromAString_noExpiration) {
  std::string cookie_string = "foo=bar; path=/; domain=chromium.org; secure;";
  Cookie cookie(cookie_string);

  EXPECT_TRUE(cookie.valid());
  EXPECT_EQ("foo", cookie.name());
  EXPECT_EQ("bar", cookie.value());
  EXPECT_EQ("/", cookie.path());
  EXPECT_EQ("chromium.org", cookie.domain());
  EXPECT_TRUE(cookie.secure());
  EXPECT_FALSE(cookie.http_only());
  EXPECT_TRUE(cookie.expiration().is_null());

  EXPECT_EQ(cookie_string, cookie.ToString());

  scoped_ptr<DictionaryValue> dict(cookie.ToDictionary());
  EXPECT_EQ(6u, dict->size());
  ExpectStringEq(dict, "name", "foo");
  ExpectStringEq(dict, "value", "bar");
  ExpectStringEq(dict, "path", "/");
  ExpectStringEq(dict, "domain", "chromium.org");
  ExpectBooleanEq(dict, "secure", true);
  ExpectBooleanEq(dict, "http_only", false);
  EXPECT_FALSE(dict->HasKey("expiry"));
}

TEST(CookieTest, CreatingCookieFromWebDriverDictionary_FloatingPointExpiry) {
  DictionaryValue dict;
  dict.SetString("name", "foo");
  dict.SetString("value", "bar");
  dict.SetString("path", "/");
  dict.SetString("domain", "chromium.org");
  dict.SetBoolean("secure", true);
  dict.SetDouble("expiry", kArbitraryDateInSecondsSinceEpochF);

  Cookie cookie(dict);
  EXPECT_TRUE(cookie.valid());
  EXPECT_EQ("foo", cookie.name());
  EXPECT_EQ("bar", cookie.value());
  EXPECT_EQ("/", cookie.path());
  EXPECT_EQ("chromium.org", cookie.domain());
  EXPECT_TRUE(cookie.secure());
  EXPECT_FALSE(cookie.http_only());
  EXPECT_EQ(kArbitraryDate, cookie.expiration());

  EXPECT_EQ(cookie.ToString(), base::StringPrintf(
      "foo=bar; path=/; domain=chromium.org; expires=%s; secure;",
      kArbitraryDateString));

  scoped_ptr<DictionaryValue> reconstituted_dict(cookie.ToDictionary());
  EXPECT_EQ(7u, reconstituted_dict->size());
  ExpectStringEq(reconstituted_dict, "name", "foo");
  ExpectStringEq(reconstituted_dict, "value", "bar");
  ExpectStringEq(reconstituted_dict, "path", "/");
  ExpectStringEq(reconstituted_dict, "domain", "chromium.org");
  ExpectBooleanEq(reconstituted_dict, "secure", true);
  ExpectBooleanEq(reconstituted_dict, "http_only", false);
  ExpectDoubleEq(reconstituted_dict, "expiry",
                 kArbitraryDateInSecondsSinceEpochF);
}

TEST(CookieTest, CreatingCookieFromWebDriverDictionary_IntegerExpiry) {
  DictionaryValue dict;
  dict.SetString("name", "foo");
  dict.SetString("value", "bar");
  dict.SetString("path", "/");
  dict.SetString("domain", "chromium.org");
  dict.SetBoolean("secure", true);
  dict.SetInteger("expiry", kArbitraryDateInSecondsSinceEpoch);

  Cookie cookie(dict);
  EXPECT_TRUE(cookie.valid());
  EXPECT_EQ("foo", cookie.name());
  EXPECT_EQ("bar", cookie.value());
  EXPECT_EQ("/", cookie.path());
  EXPECT_EQ("chromium.org", cookie.domain());
  EXPECT_TRUE(cookie.secure());
  EXPECT_FALSE(cookie.http_only());
  EXPECT_EQ(kArbitraryDate, cookie.expiration());

  EXPECT_EQ(cookie.ToString(), base::StringPrintf(
      "foo=bar; path=/; domain=chromium.org; expires=%s; secure;",
      kArbitraryDateString));

  scoped_ptr<DictionaryValue> reconstituted_dict(cookie.ToDictionary());
  EXPECT_EQ(7u, reconstituted_dict->size());
  ExpectStringEq(reconstituted_dict, "name", "foo");
  ExpectStringEq(reconstituted_dict, "value", "bar");
  ExpectStringEq(reconstituted_dict, "path", "/");
  ExpectStringEq(reconstituted_dict, "domain", "chromium.org");
  ExpectBooleanEq(reconstituted_dict, "secure", true);
  ExpectBooleanEq(reconstituted_dict, "http_only", false);
  ExpectDoubleEq(reconstituted_dict, "expiry",
                 kArbitraryDateInSecondsSinceEpochF);
}

TEST(CookieTest, UsesEmptyDomainInStringFormatIfDomainIsLocalhost) {
  DictionaryValue dict;
  dict.SetString("name", "foo");
  dict.SetString("value", "bar");
  dict.SetString("domain", "localhost");

  Cookie cookie(dict);
  EXPECT_TRUE(cookie.valid());
  EXPECT_EQ("foo", cookie.name());
  EXPECT_EQ("bar", cookie.value());
  EXPECT_EQ("localhost", cookie.domain());

  EXPECT_EQ(cookie.ToString(), "foo=bar; domain=;");
}

}  // namespace webdriver
