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
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::ElementsAre;
using testing::HasSubstr;
using url_matcher::URLMatcher;
using url_matcher::URLMatcherConditionSet;

namespace extensions {

namespace {

scoped_refptr<Extension> CreateExtensionWithBookmarksPermission(
    bool include_bookmarks) {
  ListBuilder permissions;
  permissions.Append("declarativeContent");
  if (include_bookmarks)
    permissions.Append("bookmarks");
  return ExtensionBuilder()
      .SetManifest(DictionaryBuilder()
          .Set("name", "Test extension")
          .Set("version", "1.0")
          .Set("manifest_version", 2)
          .Set("permissions", permissions))
          .Build();
}

}  // namespace

TEST(DeclarativeContentConditionTest, UnknownConditionName) {
  URLMatcher matcher;
  std::string error;
  scoped_ptr<ContentCondition> result = CreateContentCondition(
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
  scoped_ptr<ContentCondition> result = CreateContentCondition(
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
  scoped_ptr<ContentCondition> result = CreateContentCondition(
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

TEST(DeclarativeContentConditionTest, ConditionWithUrl) {
  URLMatcher matcher;
  scoped_refptr<Extension> extension =
      CreateExtensionWithBookmarksPermission(false);

  std::string error;
  scoped_ptr<ContentCondition> condition = CreateContentCondition(
      extension.get(),
      matcher.condition_factory(),
      *base::test::ParseJson(
          "{\n"
          "  \"instanceType\": \"declarativeContent.PageStateMatcher\",\n"
          "  \"pageUrl\": {\"hostSuffix\": \"example.com\"},\n"
          "}"),
      &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(condition);

  URLMatcherConditionSet::Vector all_new_condition_sets;
  all_new_condition_sets.push_back(
      condition->page_url_predicate->url_matcher_condition_set());
  matcher.AddConditionSets(all_new_condition_sets);
  EXPECT_FALSE(matcher.IsEmpty());

  EXPECT_THAT(matcher.MatchURL(GURL("http://google.com/")),
              ElementsAre(/*empty*/));
  std::set<url_matcher::URLMatcherConditionSet::ID> page_url_matches =
      matcher.MatchURL(GURL("http://www.example.com/foobar"));
  EXPECT_THAT(
      page_url_matches,
      ElementsAre(
          condition->page_url_predicate->url_matcher_condition_set()->id()));

  EXPECT_TRUE(condition->page_url_predicate->Evaluate(page_url_matches));
}

TEST(DeclarativeContentConditionTest, ConditionWithCss) {
  URLMatcher matcher;
  scoped_refptr<Extension> extension =
      CreateExtensionWithBookmarksPermission(false);

  std::string error;
  scoped_ptr<ContentCondition> condition = CreateContentCondition(
      extension.get(),
      matcher.condition_factory(),
      *base::test::ParseJson(
          "{\n"
          "  \"instanceType\": \"declarativeContent.PageStateMatcher\",\n"
          "  \"css\": [\"input\"],\n"
          "}"),
      &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(condition);

  base::hash_set<std::string> matched_css_selectors;
  matched_css_selectors.insert("input");

  EXPECT_TRUE(condition->css_predicate->Evaluate(matched_css_selectors));

  matched_css_selectors.clear();
  matched_css_selectors.insert("body");
  EXPECT_FALSE(condition->css_predicate->Evaluate(matched_css_selectors));
}

// Tests that condition with isBookmarked requires "bookmarks" permission.
TEST(DeclarativeContentConditionTest, IsBookmarkedRequiresBookmarkPermission) {
  URLMatcher matcher;
  scoped_refptr<Extension> extension =
      CreateExtensionWithBookmarksPermission(false);

  std::string error;
  scoped_ptr<ContentCondition> condition = CreateContentCondition(
      extension.get(),
      matcher.condition_factory(),
      *base::test::ParseJson(
          "{\n"
          "  \"isBookmarked\": true,\n"
          "  \"instanceType\": \"declarativeContent.PageStateMatcher\",\n"
          "}"),
      &error);
  EXPECT_THAT(error, HasSubstr("requires 'bookmarks' permission"));
  ASSERT_FALSE(condition);
}

// Tests an invalid isBookmarked value type.
TEST(DeclarativeContentConditionTest, WrongIsBookmarkedDatatype) {
  URLMatcher matcher;
  scoped_refptr<Extension> extension =
      CreateExtensionWithBookmarksPermission(true);

  std::string error;
  scoped_ptr<ContentCondition> condition = CreateContentCondition(
      extension.get(),
      matcher.condition_factory(),
      *base::test::ParseJson(
          "{\n"
          "  \"isBookmarked\": [],\n"
          "  \"instanceType\": \"declarativeContent.PageStateMatcher\",\n"
          "}"),
      &error);
  EXPECT_THAT(error, HasSubstr("invalid type"));
  EXPECT_FALSE(condition);
}

// Tests isBookmark: true.
TEST(DeclarativeContentConditionTest, IsBookmarkedTrue) {
  URLMatcher matcher;
  scoped_refptr<Extension> extension =
      CreateExtensionWithBookmarksPermission(true);

  std::string error;
  scoped_ptr<ContentCondition> condition = CreateContentCondition(
      extension.get(),
      matcher.condition_factory(),
      *base::test::ParseJson(
          "{\n"
          "  \"isBookmarked\": true,\n"
          "  \"instanceType\": \"declarativeContent.PageStateMatcher\",\n"
          "}"),
      &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(condition);

  EXPECT_TRUE(condition->is_bookmarked_predicate->Evaluate(
      true /* url_is_bookmarked */));
  EXPECT_FALSE(condition->is_bookmarked_predicate->Evaluate(
      false /* url_is_bookmarked */));
}

// Tests isBookmark: false.
TEST(DeclarativeContentConditionTest, IsBookmarkedFalse) {
  URLMatcher matcher;
  scoped_refptr<Extension> extension =
      CreateExtensionWithBookmarksPermission(true);

  std::string error;
  scoped_ptr<ContentCondition> condition = CreateContentCondition(
      extension.get(),
      matcher.condition_factory(),
      *base::test::ParseJson(
          "{\n"
          "  \"isBookmarked\": false,\n"
          "  \"instanceType\": \"declarativeContent.PageStateMatcher\",\n"
          "}"),
      &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(condition);

  EXPECT_FALSE(condition->is_bookmarked_predicate->Evaluate(
      true /* url_is_bookmarked */));
  EXPECT_TRUE(condition->is_bookmarked_predicate->Evaluate(
      false /* url_is_bookmarked */));
}

}  // namespace extensions
