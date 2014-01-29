// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/content_condition.h"

#include <set>

#include "base/message_loop/message_loop.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/declarative_content/content_constants.h"
#include "components/url_matcher/url_matcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::ElementsAre;
using testing::HasSubstr;
using url_matcher::URLMatcher;
using url_matcher::URLMatcherConditionSet;

namespace extensions {
namespace {

TEST(DeclarativeContentConditionTest, UnknownConditionName) {
  URLMatcher matcher;
  std::string error;
  scoped_ptr<ContentCondition> result = ContentCondition::Create(
      NULL,
      matcher.condition_factory(),
      *base::test::ParseJson(
           "{\n"
           "  \"invalid\": \"foobar\",\n"
           "  \"instanceType\": \"declarativeContent.PageStateMatcher\",\n"
           "}"),
      &error);
  EXPECT_THAT(error, HasSubstr("Unknown condition attribute"));
  EXPECT_FALSE(result);

  EXPECT_TRUE(matcher.IsEmpty()) << "Errors shouldn't add URL conditions";
}

TEST(DeclarativeContentConditionTest, WrongPageUrlDatatype) {
  URLMatcher matcher;
  std::string error;
  scoped_ptr<ContentCondition> result = ContentCondition::Create(
      NULL,
      matcher.condition_factory(),
      *base::test::ParseJson(
          "{\n"
          "  \"pageUrl\": [],\n"
          "  \"instanceType\": \"declarativeContent.PageStateMatcher\",\n"
          "}"),
      &error);
  EXPECT_THAT(error, HasSubstr("invalid type"));
  EXPECT_FALSE(result);

  EXPECT_TRUE(matcher.IsEmpty()) << "Errors shouldn't add URL conditions";
}

TEST(DeclarativeContentConditionTest, WrongCssDatatype) {
  URLMatcher matcher;
  std::string error;
  scoped_ptr<ContentCondition> result = ContentCondition::Create(
      NULL,
      matcher.condition_factory(),
      *base::test::ParseJson(
          "{\n"
          "  \"css\": \"selector\",\n"
          "  \"instanceType\": \"declarativeContent.PageStateMatcher\",\n"
          "}"),
      &error);
  EXPECT_THAT(error, HasSubstr("invalid type"));
  EXPECT_FALSE(result);

  EXPECT_TRUE(matcher.IsEmpty()) << "Errors shouldn't add URL conditions";
}

TEST(DeclarativeContentConditionTest, ConditionWithUrlAndCss) {
  URLMatcher matcher;

  std::string error;
  scoped_ptr<ContentCondition> result = ContentCondition::Create(
      NULL,
      matcher.condition_factory(),
      *base::test::ParseJson(
          "{\n"
          "  \"instanceType\": \"declarativeContent.PageStateMatcher\",\n"
          "  \"pageUrl\": {\"hostSuffix\": \"example.com\"},\n"
          "  \"css\": [\"input\"],\n"
          "}"),
      &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(result);

  URLMatcherConditionSet::Vector all_new_condition_sets;
  result->GetURLMatcherConditionSets(&all_new_condition_sets);
  matcher.AddConditionSets(all_new_condition_sets);
  EXPECT_FALSE(matcher.IsEmpty());

  RendererContentMatchData match_data;
  match_data.css_selectors.insert("input");

  EXPECT_THAT(matcher.MatchURL(GURL("http://google.com/")),
              ElementsAre(/*empty*/));
  match_data.page_url_matches = matcher.MatchURL(
      GURL("http://www.example.com/foobar"));
  EXPECT_THAT(match_data.page_url_matches,
              ElementsAre(result->url_matcher_condition_set_id()));

  EXPECT_TRUE(result->IsFulfilled(match_data));

  match_data.css_selectors.clear();
  match_data.css_selectors.insert("body");
  EXPECT_FALSE(result->IsFulfilled(match_data));
}

}  // namespace
}  // namespace extensions
