// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/matcher/url_matcher_factory.h"

#include "base/format_macros.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/common/extensions/matcher/url_matcher_constants.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace keys = url_matcher_constants;

TEST(URLMatcherFactoryTest, CreateFromURLFilterDictionary) {
  URLMatcher matcher;

  std::string error;
  scoped_refptr<URLMatcherConditionSet> result;

  // Invalid key: {"invalid": "foobar"}
  DictionaryValue invalid_condition;
  invalid_condition.SetString("invalid", "foobar");

  // Invalid value type: {"hostSuffix": []}
  DictionaryValue invalid_condition2;
  invalid_condition2.Set(keys::kHostSuffixKey, new ListValue);

  // Invalid regex value: {"urlMatches": "*"}
  DictionaryValue invalid_condition3;
  invalid_condition3.SetString(keys::kURLMatchesKey, "*");

  // Valid values:
  // {
  //   "port_range": [80, [1000, 1010]],
  //   "schemes": ["http"],
  //   "hostSuffix": "example.com"
  //   "hostPrefix": "www"
  // }

  // Port range: Allow 80;1000-1010.
  ListValue* port_range = new ListValue();
  port_range->Append(Value::CreateIntegerValue(1000));
  port_range->Append(Value::CreateIntegerValue(1010));
  ListValue* port_ranges = new ListValue();
  port_ranges->Append(Value::CreateIntegerValue(80));
  port_ranges->Append(port_range);

  ListValue* scheme_list = new ListValue();
  scheme_list->Append(Value::CreateStringValue("http"));

  DictionaryValue valid_condition;
  valid_condition.SetString(keys::kHostSuffixKey, "example.com");
  valid_condition.SetString(keys::kHostPrefixKey, "www");
  valid_condition.Set(keys::kPortsKey, port_ranges);
  valid_condition.Set(keys::kSchemesKey, scheme_list);

  // Test wrong condition name passed.
  error.clear();
  result = URLMatcherFactory::CreateFromURLFilterDictionary(
      matcher.condition_factory(), &invalid_condition, 1, &error);
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(result.get());

  // Test wrong datatype in hostSuffix.
  error.clear();
  result = URLMatcherFactory::CreateFromURLFilterDictionary(
      matcher.condition_factory(), &invalid_condition2, 2, &error);
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(result.get());

  // Test invalid regex in urlMatches.
  error.clear();
  result = URLMatcherFactory::CreateFromURLFilterDictionary(
      matcher.condition_factory(), &invalid_condition3, 3, &error);
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(result.get());

  // Test success.
  error.clear();
  result = URLMatcherFactory::CreateFromURLFilterDictionary(
      matcher.condition_factory(), &valid_condition, 100, &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(result.get());

  URLMatcherConditionSet::Vector conditions;
  conditions.push_back(result);
  matcher.AddConditionSets(conditions);

  EXPECT_EQ(1u, matcher.MatchURL(GURL("http://www.example.com")).size());
  EXPECT_EQ(1u, matcher.MatchURL(GURL("http://www.example.com:80")).size());
  EXPECT_EQ(1u, matcher.MatchURL(GURL("http://www.example.com:1000")).size());
  // Wrong scheme.
  EXPECT_EQ(0u, matcher.MatchURL(GURL("https://www.example.com:80")).size());
  // Wrong port.
  EXPECT_EQ(0u, matcher.MatchURL(GURL("http://www.example.com:81")).size());
  // Unfulfilled host prefix.
  EXPECT_EQ(0u, matcher.MatchURL(GURL("http://mail.example.com:81")).size());
}

// This class wraps a case sensitivity test for a single UrlFilter condition.
class UrlConditionCaseTest {
 public:
  // The condition is identified by the key |condition_key|. If that key is
  // associated with string values, then |use_list_of_strings| should be false,
  // if the key is associated with list-of-string values, then
  // |use_list_of_strings| should be true. In |url| is the URL to test against.
  UrlConditionCaseTest(const char* condition_key,
                       const bool use_list_of_strings,
                       const std::string& expected_value,
                       const std::string& incorrect_case_value,
                       const bool case_sensitive,
                       const GURL& url)
      : condition_key_(condition_key),
        use_list_of_strings_(use_list_of_strings),
        expected_value_(expected_value),
        incorrect_case_value_(incorrect_case_value),
        case_sensitive_(case_sensitive),
        url_(url) {}

  ~UrlConditionCaseTest() {}

  // Match the condition against |url_|. Checks via EXPECT_* macros that
  // |expected_value_| matches always, and that |incorrect_case_value_| matches
  // iff |case_sensitive_| is false.
  void Test() const;

 private:
  // Check, via EXPECT_* macros, that the the condition |condition_key_|=|value|
  // fails against |url_| iff |should_fail| is true. This check is expensive,
  // its value should be cached if needed multiple times.
  void CheckCondition(const std::string& value, bool should_fail) const;

  const char* condition_key_;
  const bool use_list_of_strings_;
  const std::string& expected_value_;
  const std::string& incorrect_case_value_;
  const bool case_sensitive_;
  const GURL& url_;

  // Allow implicit copy and assign, because a public copy constructor is
  // needed, but never used (!), for the definition of arrays of this class.
};

void UrlConditionCaseTest::Test() const {
  CheckCondition(expected_value_, false);
  CheckCondition(incorrect_case_value_, case_sensitive_);
}

void UrlConditionCaseTest::CheckCondition(const std::string& value,
                                          bool should_fail) const {
  DictionaryValue condition;
  if (use_list_of_strings_) {
    ListValue* list = new ListValue();
    list->Append(Value::CreateStringValue(value));
    condition.SetWithoutPathExpansion(condition_key_, list);
  } else {
    condition.SetStringWithoutPathExpansion(condition_key_, value);
  }

  URLMatcher matcher;
  std::string error;
  scoped_refptr<URLMatcherConditionSet> result;

  result = URLMatcherFactory::CreateFromURLFilterDictionary(
      matcher.condition_factory(), &condition, 1, &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(result.get());

  URLMatcherConditionSet::Vector conditions;
  conditions.push_back(result);
  matcher.AddConditionSets(conditions);
  EXPECT_EQ((should_fail ? 0u : 1u), matcher.MatchURL(url_).size())
      << "while matching condition " << condition_key_ << " with value "
      << value  << " against url " << url_;
}

// This tests that the UrlFilter handles case sensitivity on various parts of
// URLs correctly.
TEST(URLMatcherFactoryTest, CaseSensitivity) {
  const std::string kScheme("https");
  const std::string kSchemeUpper("HTTPS");
  const std::string kHost("www.example.com");
  const std::string kHostUpper("WWW.EXAMPLE.COM");
  const std::string kPath("/path");
  const std::string kPathUpper("/PATH");
  const std::string kQuery("?option=value&A=B");
  const std::string kQueryUpper("?OPTION=VALUE&A=B");
  const std::string kUrl(kScheme + "://" + kHost + ":1234" + kPath + kQuery);
  const std::string kUrlUpper(
      kSchemeUpper + "://" + kHostUpper + ":1234" + kPathUpper + kQueryUpper);
  const GURL url(kUrl);
  // Note: according to RFC 3986, and RFC 1034, schema and host, respectively
  // should be case insensitive. See crbug.com/160702, comments 6 and 7, for why
  // we still require them to be case sensitive in UrlFilter.
  const bool kIsSchemeCaseSensitive = true;
  const bool kIsHostCaseSensitive = true;
  const bool kIsPathCaseSensitive = true;
  const bool kIsQueryCaseSensitive = true;
  const bool kIsUrlCaseSensitive = kIsSchemeCaseSensitive ||
      kIsHostCaseSensitive || kIsPathCaseSensitive || kIsQueryCaseSensitive;

  const UrlConditionCaseTest case_tests[] = {
    UrlConditionCaseTest(keys::kSchemesKey, true, kScheme, kSchemeUpper,
                         kIsSchemeCaseSensitive, url),
    UrlConditionCaseTest(keys::kHostContainsKey, false, kHost, kHostUpper,
                         kIsHostCaseSensitive, url),
    UrlConditionCaseTest(keys::kHostEqualsKey, false, kHost, kHostUpper,
                         kIsHostCaseSensitive, url),
    UrlConditionCaseTest(keys::kHostPrefixKey, false, kHost, kHostUpper,
                         kIsHostCaseSensitive, url),
    UrlConditionCaseTest(keys::kHostSuffixKey, false, kHost, kHostUpper,
                         kIsHostCaseSensitive, url),
    UrlConditionCaseTest(keys::kPathContainsKey, false, kPath, kPathUpper,
                         kIsPathCaseSensitive, url),
    UrlConditionCaseTest(keys::kPathEqualsKey, false, kPath, kPathUpper,
                         kIsPathCaseSensitive, url),
    UrlConditionCaseTest(keys::kPathPrefixKey, false, kPath, kPathUpper,
                         kIsPathCaseSensitive, url),
    UrlConditionCaseTest(keys::kPathSuffixKey, false, kPath, kPathUpper,
                         kIsPathCaseSensitive, url),
    UrlConditionCaseTest(keys::kQueryContainsKey, false, kQuery, kQueryUpper,
                         kIsQueryCaseSensitive, url),
    UrlConditionCaseTest(keys::kQueryEqualsKey, false, kQuery, kQueryUpper,
                         kIsQueryCaseSensitive, url),
    UrlConditionCaseTest(keys::kQueryPrefixKey, false, kQuery, kQueryUpper,
                         kIsQueryCaseSensitive, url),
    UrlConditionCaseTest(keys::kQuerySuffixKey, false, kQuery, kQueryUpper,
                         kIsQueryCaseSensitive, url),
    // Excluding kURLMatchesKey because case sensitivity can be specified in the
    // RE2 expression.
    UrlConditionCaseTest(keys::kURLContainsKey, false, kUrl, kUrlUpper,
                         kIsUrlCaseSensitive, url),
    UrlConditionCaseTest(keys::kURLEqualsKey, false, kUrl, kUrlUpper,
                         kIsUrlCaseSensitive, url),
    UrlConditionCaseTest(keys::kURLPrefixKey, false, kUrl, kUrlUpper,
                         kIsUrlCaseSensitive, url),
    UrlConditionCaseTest(keys::kURLSuffixKey, false, kUrl, kUrlUpper,
                         kIsUrlCaseSensitive, url),
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(case_tests); ++i) {
    SCOPED_TRACE(base::StringPrintf("Iteration: %" PRIuS, i));
    case_tests[i].Test();
  }
}

}  // namespace extensions
