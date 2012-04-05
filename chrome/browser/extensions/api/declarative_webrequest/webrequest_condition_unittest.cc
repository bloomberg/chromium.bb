// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_condition.h"

#include <set>

#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_constants.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace keys = declarative_webrequest_constants;

TEST(WebRequestConditionTest, CreateCondition) {
  // Necessary for TestURLRequest.
  MessageLoop message_loop(MessageLoop::TYPE_IO);
  URLMatcher matcher;

  std::string error;
  scoped_ptr<WebRequestCondition> result;

  DictionaryValue invalid_condition;
  invalid_condition.SetString("invalid", "foobar");
  invalid_condition.SetString("host_suffix", "example.com");
  invalid_condition.SetString(keys::kInstanceTypeKey,
                              keys::kRequestMatcherType);

  DictionaryValue invalid_condition2;
  invalid_condition2.Set("host_suffix", new ListValue);
  invalid_condition2.SetString(keys::kInstanceTypeKey,
                               keys::kRequestMatcherType);

  DictionaryValue valid_condition;
  valid_condition.SetString("scheme", "http");
  valid_condition.SetString("host_suffix", "example.com");
  valid_condition.SetString(keys::kInstanceTypeKey,
                            keys::kRequestMatcherType);

  // Test wrong condition name passed.
  error.clear();
  result = WebRequestCondition::Create(matcher.condition_factory(),
                                       invalid_condition, &error);
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(result.get());

  // Test wrong datatype in host_suffix.
  error.clear();
  result = WebRequestCondition::Create(matcher.condition_factory(),
                                       invalid_condition2, &error);
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(result.get());

  // Test success (can we support multiple criteria?)
  error.clear();
  result = WebRequestCondition::Create(matcher.condition_factory(),
                                       valid_condition, &error);
  EXPECT_TRUE(error.empty());
  ASSERT_TRUE(result.get());

  TestURLRequest match_request(GURL("http://www.example.com"), NULL);
  EXPECT_TRUE(result->IsFulfilled(&match_request, ON_BEFORE_REQUEST));

  TestURLRequest wrong_scheme(GURL("https://www.example.com"), NULL);
  EXPECT_FALSE(result->IsFulfilled(&wrong_scheme, ON_BEFORE_REQUEST));
}

TEST(WebRequestConditionTest, CreateConditionSet) {
  // Necessary for TestURLRequest.
  MessageLoop message_loop(MessageLoop::TYPE_IO);
  URLMatcher matcher;

  DictionaryValue http_condition;
  http_condition.SetString("scheme", "http");
  http_condition.SetString("host_suffix", "example.com");
  http_condition.SetString(keys::kInstanceTypeKey, keys::kRequestMatcherType);

  DictionaryValue https_condition;
  https_condition.SetString("scheme", "https");
  https_condition.SetString("host_suffix", "example.com");
  https_condition.SetString("host_prefix", "www");
  https_condition.SetString(keys::kInstanceTypeKey, keys::kRequestMatcherType);

  WebRequestConditionSet::AnyVector conditions;

  linked_ptr<json_schema_compiler::any::Any> condition1 = make_linked_ptr(
      new json_schema_compiler::any::Any);
  condition1->Init(http_condition);
  conditions.push_back(condition1);

  linked_ptr<json_schema_compiler::any::Any> condition2 = make_linked_ptr(
      new json_schema_compiler::any::Any);
  condition2->Init(https_condition);
  conditions.push_back(condition2);

  // Test insertion
  std::string error;
  scoped_ptr<WebRequestConditionSet> result =
      WebRequestConditionSet::Create(matcher.condition_factory(),
                                     conditions, &error);
  EXPECT_TRUE(error.empty());
  ASSERT_TRUE(result.get());
  EXPECT_EQ(2u, result->conditions().size());

  // Tell the URLMatcher about our shiny new patterns.
  URLMatcherConditionSet::Vector url_matcher_condition_set;
  result->GetURLMatcherConditionSets(&url_matcher_condition_set);
  matcher.AddConditionSets(url_matcher_condition_set);

  std::set<URLMatcherConditionSet::ID> url_match_ids;
  int number_matches = 0;

  // Test that the result is correct and matches http://www.example.com and
  // https://www.example.com
  GURL http_url("http://www.example.com");
  TestURLRequest http_request(http_url, NULL);
  url_match_ids = matcher.MatchURL(http_url);
  for (std::set<URLMatcherConditionSet::ID>::iterator i = url_match_ids.begin();
       i != url_match_ids.end(); ++i) {
    if (result->IsFulfilled(*i, &http_request, ON_BEFORE_REQUEST))
      ++number_matches;
  }
  EXPECT_EQ(1, number_matches);

  GURL https_url("https://www.example.com");
  url_match_ids = matcher.MatchURL(https_url);
  TestURLRequest https_request(https_url, NULL);
  number_matches = 0;
  for (std::set<URLMatcherConditionSet::ID>::iterator i = url_match_ids.begin();
       i != url_match_ids.end(); ++i) {
    if (result->IsFulfilled(*i, &https_request, ON_BEFORE_REQUEST))
      ++number_matches;
  }
  EXPECT_EQ(1, number_matches);

  // Check that both, host_prefix and host_suffix are evaluated.
  GURL https_foo_url("https://foo.example.com");
  url_match_ids = matcher.MatchURL(https_foo_url);
  TestURLRequest https_foo_request(https_foo_url, NULL);
  number_matches = 0;
  for (std::set<URLMatcherConditionSet::ID>::iterator i = url_match_ids.begin();
       i != url_match_ids.end(); ++i) {
    if (result->IsFulfilled(*i, &https_foo_request, ON_BEFORE_REQUEST))
      ++number_matches;
  }
  EXPECT_EQ(0, number_matches);
}

}  // namespace extensions
