// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_condition.h"

#include <set>

#include "base/message_loop.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_constants.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_rule.h"
#include "chrome/common/extensions/matcher/url_matcher_constants.h"
#include "content/public/browser/resource_request_info.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace keys = declarative_webrequest_constants;
namespace keys2 = url_matcher_constants;

TEST(WebRequestConditionTest, CreateCondition) {
  // Necessary for TestURLRequest.
  MessageLoop message_loop(MessageLoop::TYPE_IO);
  URLMatcher matcher;

  std::string error;
  scoped_ptr<WebRequestCondition> result;


  // Test wrong condition name passed.
  error.clear();
  result = WebRequestCondition::Create(
      matcher.condition_factory(),
      *base::test::ParseJson(
          "{ \"invalid\": \"foobar\", \n"
          "  \"instanceType\": \"declarativeWebRequest.RequestMatcher\", \n"
          "}"),
      &error);
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(result.get());

  // Test wrong datatype in host_suffix.
  error.clear();
  result = WebRequestCondition::Create(
      matcher.condition_factory(),
      *base::test::ParseJson(
          "{ \n"
          "  \"url\": [], \n"
          "  \"instanceType\": \"declarativeWebRequest.RequestMatcher\", \n"
          "}"),
      &error);
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(result.get());

  // Test success (can we support multiple criteria?)
  error.clear();
  result = WebRequestCondition::Create(
      matcher.condition_factory(),
      *base::test::ParseJson(
          "{ \n"
          "  \"resourceType\": [\"main_frame\"], \n"
          "  \"url\": { \"hostSuffix\": \"example.com\" }, \n"
          "  \"instanceType\": \"declarativeWebRequest.RequestMatcher\", \n"
          "}"),
      &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(result.get());

  net::TestURLRequestContext context;
  net::TestURLRequest match_request(
      GURL("http://www.example.com"), NULL, &context);
  content::ResourceRequestInfo::AllocateForTesting(&match_request,
      ResourceType::MAIN_FRAME, NULL, -1, -1);
  EXPECT_TRUE(result->IsFulfilled(
      WebRequestRule::RequestData(&match_request, ON_BEFORE_REQUEST)));

  net::TestURLRequest wrong_resource_type(
      GURL("https://www.example.com"), NULL, &context);
  content::ResourceRequestInfo::AllocateForTesting(&wrong_resource_type,
      ResourceType::SUB_FRAME, NULL, -1, -1);
  EXPECT_FALSE(result->IsFulfilled(
      WebRequestRule::RequestData(&wrong_resource_type, ON_BEFORE_REQUEST)));
}

TEST(WebRequestConditionTest, CreateConditionSet) {
  // Necessary for TestURLRequest.
  MessageLoop message_loop(MessageLoop::TYPE_IO);
  URLMatcher matcher;

  WebRequestConditionSet::AnyVector conditions;

  linked_ptr<json_schema_compiler::any::Any> condition1 = make_linked_ptr(
      new json_schema_compiler::any::Any);
  condition1->Init(*base::test::ParseJson(
      "{ \n"
      "  \"instanceType\": \"declarativeWebRequest.RequestMatcher\", \n"
      "  \"url\": { \n"
      "    \"hostSuffix\": \"example.com\", \n"
      "    \"schemes\": [\"http\"], \n"
      "  }, \n"
      "}"));
  conditions.push_back(condition1);

  linked_ptr<json_schema_compiler::any::Any> condition2 = make_linked_ptr(
      new json_schema_compiler::any::Any);
  condition2->Init(*base::test::ParseJson(
      "{ \n"
      "  \"instanceType\": \"declarativeWebRequest.RequestMatcher\", \n"
      "  \"url\": { \n"
      "    \"hostSuffix\": \"example.com\", \n"
      "    \"hostPrefix\": \"www\", \n"
      "    \"schemes\": [\"https\"], \n"
      "  }, \n"
      "}"));
  conditions.push_back(condition2);

  // Test insertion
  std::string error;
  scoped_ptr<WebRequestConditionSet> result =
      WebRequestConditionSet::Create(matcher.condition_factory(),
                                     conditions, &error);
  EXPECT_EQ("", error);
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
  net::TestURLRequestContext context;
  net::TestURLRequest http_request(http_url, NULL, &context);
  url_match_ids = matcher.MatchURL(http_url);
  for (std::set<URLMatcherConditionSet::ID>::iterator i = url_match_ids.begin();
       i != url_match_ids.end(); ++i) {
    if (result->IsFulfilled(
            *i, WebRequestRule::RequestData(&http_request, ON_BEFORE_REQUEST)))
      ++number_matches;
  }
  EXPECT_EQ(1, number_matches);

  GURL https_url("https://www.example.com");
  url_match_ids = matcher.MatchURL(https_url);
  net::TestURLRequest https_request(https_url, NULL, &context);
  number_matches = 0;
  for (std::set<URLMatcherConditionSet::ID>::iterator i = url_match_ids.begin();
       i != url_match_ids.end(); ++i) {
    if (result->IsFulfilled(
            *i, WebRequestRule::RequestData(&https_request, ON_BEFORE_REQUEST)))
      ++number_matches;
  }
  EXPECT_EQ(1, number_matches);

  // Check that both, hostPrefix and hostSuffix are evaluated.
  GURL https_foo_url("https://foo.example.com");
  url_match_ids = matcher.MatchURL(https_foo_url);
  net::TestURLRequest https_foo_request(https_foo_url, NULL, &context);
  number_matches = 0;
  for (std::set<URLMatcherConditionSet::ID>::iterator i = url_match_ids.begin();
       i != url_match_ids.end(); ++i) {
    if (result->IsFulfilled(
            *i, WebRequestRule::RequestData(
                &https_foo_request, ON_BEFORE_REQUEST)))
      ++number_matches;
  }
  EXPECT_EQ(0, number_matches);
}

TEST(WebRequestConditionTest, TestPortFilter) {
  // Necessary for TestURLRequest.
  MessageLoop message_loop(MessageLoop::TYPE_IO);
  URLMatcher matcher;

  linked_ptr<json_schema_compiler::any::Any> any_condition =
      make_linked_ptr(new json_schema_compiler::any::Any);
  any_condition->Init(*base::test::ParseJson(
      "{ \n"
      "  \"instanceType\": \"declarativeWebRequest.RequestMatcher\", \n"
      "  \"url\": { \n"
      "    \"ports\": [80, [1000, 1010]], \n"  // Allow 80;1000-1010.
      "    \"hostSuffix\": \"example.com\", \n"
      "  }, \n"
      "}"));
  WebRequestConditionSet::AnyVector conditions;
  conditions.push_back(any_condition);

  // Test insertion
  std::string error;
  scoped_ptr<WebRequestConditionSet> result =
      WebRequestConditionSet::Create(matcher.condition_factory(),
                                     conditions, &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(result.get());
  EXPECT_EQ(1u, result->conditions().size());

  // Tell the URLMatcher about our shiny new patterns.
  URLMatcherConditionSet::Vector url_matcher_condition_set;
  result->GetURLMatcherConditionSets(&url_matcher_condition_set);
  matcher.AddConditionSets(url_matcher_condition_set);

  std::set<URLMatcherConditionSet::ID> url_match_ids;

  // Test various URLs.
  GURL http_url("http://www.example.com");
  net::TestURLRequestContext context;
  net::TestURLRequest http_request(http_url, NULL, &context);
  url_match_ids = matcher.MatchURL(http_url);
  ASSERT_EQ(1u, url_match_ids.size());

  GURL http_url_80("http://www.example.com:80");
  net::TestURLRequest http_request_80(http_url_80, NULL, &context);
  url_match_ids = matcher.MatchURL(http_url_80);
  ASSERT_EQ(1u, url_match_ids.size());

  GURL http_url_1000("http://www.example.com:1000");
  net::TestURLRequest http_request_1000(http_url_1000, NULL, &context);
  url_match_ids = matcher.MatchURL(http_url_1000);
  ASSERT_EQ(1u, url_match_ids.size());

  GURL http_url_2000("http://www.example.com:2000");
  net::TestURLRequest http_request_2000(http_url_2000, NULL, &context);
  url_match_ids = matcher.MatchURL(http_url_2000);
  ASSERT_EQ(0u, url_match_ids.size());
}

// Create a condition with two attributes: one on the request header and one on
// the response header. The Create() method should fail and complain that it is
// impossible that both conditions are fulfilled at the same time.
TEST(WebRequestConditionTest, ConditionsWithConflictingStages) {
  // Necessary for TestURLRequest.
  MessageLoop message_loop(MessageLoop::TYPE_IO);
  URLMatcher matcher;

  std::string error;
  scoped_ptr<WebRequestCondition> result;

  DictionaryValue condition_value;
  condition_value.SetString(keys::kInstanceTypeKey, keys::kRequestMatcherType);


  // Test error on incompatible application stages for involved attributes.
  error.clear();
  result = WebRequestCondition::Create(
      matcher.condition_factory(),
      *base::test::ParseJson(
          "{ \n"
          "  \"instanceType\": \"declarativeWebRequest.RequestMatcher\", \n"
          // Pass a JS array with one empty object to each of the header
          // filters.
          "  \"requestHeaders\": [{}], \n"
          "  \"responseHeaders\": [{}], \n"
          "}"),
      &error);
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(result.get());
}

}  // namespace extensions
