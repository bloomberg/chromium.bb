// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/matcher/url_matcher_factory.h"

#include "base/values.h"
#include "chrome/common/extensions/matcher/url_matcher_constants.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace keys = url_matcher_constants;

TEST(URLMatcherFactory, CreateFromURLFilterDictionary) {
  URLMatcher matcher;

  std::string error;
  scoped_refptr<URLMatcherConditionSet> result;

  // Invalid key: {"invalid": "foobar"}
  DictionaryValue invalid_condition;
  invalid_condition.SetString("invalid", "foobar");

  // Invalid value type: {"hostSuffix": []}
  DictionaryValue invalid_condition2;
  invalid_condition2.Set(keys::kHostSuffixKey, new ListValue);

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

  // Test success.
  error.clear();
  result = URLMatcherFactory::CreateFromURLFilterDictionary(
      matcher.condition_factory(), &valid_condition, 3, &error);
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

}  // namespace extensions
