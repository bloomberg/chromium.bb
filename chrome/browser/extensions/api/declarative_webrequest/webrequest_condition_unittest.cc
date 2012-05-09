// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_condition.h"

#include <set>

#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_constants.h"
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

  DictionaryValue invalid_condition;
  invalid_condition.SetString("invalid", "foobar");
  invalid_condition.SetString(keys::kInstanceTypeKey,
                              keys::kRequestMatcherType);

  DictionaryValue invalid_condition2;
  invalid_condition2.Set(keys::kUrlKey, new ListValue);
  invalid_condition2.SetString(keys::kInstanceTypeKey,
                               keys::kRequestMatcherType);

  ListValue* resource_type_list = new ListValue();
  resource_type_list->Append(Value::CreateStringValue("main_frame"));
  DictionaryValue* url_filter = new DictionaryValue();
  url_filter->SetString(keys2::kHostSuffixKey, "example.com");
  DictionaryValue valid_condition;
  valid_condition.Set(keys::kResourceTypeKey, resource_type_list);
  valid_condition.Set(keys::kUrlKey, url_filter);
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
  EXPECT_EQ("", error);
  ASSERT_TRUE(result.get());

  TestURLRequest match_request(GURL("http://www.example.com"), NULL);
  content::ResourceRequestInfo::AllocateForTesting(&match_request,
      ResourceType::MAIN_FRAME, NULL);
  EXPECT_TRUE(result->IsFulfilled(&match_request, ON_BEFORE_REQUEST));

  TestURLRequest wrong_resource_type(GURL("https://www.example.com"), NULL);
  content::ResourceRequestInfo::AllocateForTesting(&wrong_resource_type,
      ResourceType::SUB_FRAME, NULL);
  EXPECT_FALSE(result->IsFulfilled(&wrong_resource_type, ON_BEFORE_REQUEST));
}

TEST(WebRequestConditionTest, CreateConditionSet) {
  // Necessary for TestURLRequest.
  MessageLoop message_loop(MessageLoop::TYPE_IO);
  URLMatcher matcher;

  ListValue* http_scheme_list = new ListValue();
  http_scheme_list->Append(Value::CreateStringValue("http"));
  DictionaryValue* http_url_filter = new DictionaryValue();
  http_url_filter->SetString(keys2::kHostSuffixKey, "example.com");
  http_url_filter->Set(keys2::kSchemesKey, http_scheme_list);
  DictionaryValue http_condition;
  http_condition.SetString(keys::kInstanceTypeKey, keys::kRequestMatcherType);
  http_condition.Set(keys::kUrlKey, http_url_filter);

  ListValue* https_scheme_list = new ListValue();
  https_scheme_list->Append(Value::CreateStringValue("https"));
  DictionaryValue* https_url_filter = new DictionaryValue();
  https_url_filter->SetString(keys2::kHostSuffixKey, "example.com");
  https_url_filter->SetString(keys2::kHostPrefixKey, "www");
  https_url_filter->Set(keys2::kSchemesKey, https_scheme_list);
  DictionaryValue https_condition;
  https_condition.SetString(keys::kInstanceTypeKey, keys::kRequestMatcherType);
  https_condition.Set(keys::kUrlKey, https_url_filter);

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

  // Check that both, hostPrefix and hostSuffix are evaluated.
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

TEST(WebRequestConditionTest, TestPortFilter) {
  // Necessary for TestURLRequest.
  MessageLoop message_loop(MessageLoop::TYPE_IO);
  URLMatcher matcher;

  // Allow 80;1000-1010.
  ListValue* port_range = new ListValue();
  port_range->Append(Value::CreateIntegerValue(1000));
  port_range->Append(Value::CreateIntegerValue(1010));
  ListValue* port_ranges = new ListValue();
  port_ranges->Append(Value::CreateIntegerValue(80));
  port_ranges->Append(port_range);

  DictionaryValue* url_filter = new DictionaryValue();
  url_filter->Set(keys2::kPortsKey, port_ranges);
  url_filter->SetString(keys2::kHostSuffixKey, "example.com");

  DictionaryValue condition;
  condition.SetString(keys::kInstanceTypeKey, keys::kRequestMatcherType);
  condition.Set(keys::kUrlKey, url_filter);

  linked_ptr<json_schema_compiler::any::Any> any_condition =
      make_linked_ptr(new json_schema_compiler::any::Any);
  any_condition->Init(condition);
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
  TestURLRequest http_request(http_url, NULL);
  url_match_ids = matcher.MatchURL(http_url);
  ASSERT_EQ(1u, url_match_ids.size());

  GURL http_url_80("http://www.example.com:80");
  TestURLRequest http_request_80(http_url_80, NULL);
  url_match_ids = matcher.MatchURL(http_url_80);
  ASSERT_EQ(1u, url_match_ids.size());

  GURL http_url_1000("http://www.example.com:1000");
  TestURLRequest http_request_1000(http_url_1000, NULL);
  url_match_ids = matcher.MatchURL(http_url_1000);
  ASSERT_EQ(1u, url_match_ids.size());

  GURL http_url_2000("http://www.example.com:2000");
  TestURLRequest http_request_2000(http_url_2000, NULL);
  url_match_ids = matcher.MatchURL(http_url_2000);
  ASSERT_EQ(0u, url_match_ids.size());
}

}  // namespace extensions
