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
  scoped_ptr<ContentCondition> condition = CreateContentCondition(
      nullptr,
      matcher.condition_factory(),
      *base::test::ParseJson(
           "{\n"
           "  \"invalid\": \"foobar\",\n"
           "  \"instanceType\": \"declarativeContent.PageStateMatcher\",\n"
           "}"),
      &error);
  EXPECT_THAT(error, HasSubstr("Unknown condition attribute"));
  EXPECT_FALSE(condition);

  EXPECT_TRUE(matcher.IsEmpty()) << "Errors shouldn't add URL conditions";
}

TEST(DeclarativeContentConditionTest,
     PredicateWithErrorProducesEmptyCondition) {
  URLMatcher matcher;
  std::string error;
  scoped_ptr<ContentCondition> condition = CreateContentCondition(
      nullptr,
      matcher.condition_factory(),
      *base::test::ParseJson(
           "{\n"
           "  \"css\": \"selector\",\n"
           "  \"instanceType\": \"declarativeContent.PageStateMatcher\",\n"
           "}"),
      &error);
  EXPECT_THAT(error, HasSubstr("invalid type"));
  EXPECT_FALSE(condition);
}

TEST(DeclarativeContentConditionTest, AllSpecifiedPredicatesCreated) {
  scoped_refptr<Extension> extension =
      CreateExtensionWithBookmarksPermission(true);
  URLMatcher matcher;
  std::string error;
  scoped_ptr<ContentCondition> condition = CreateContentCondition(
      extension.get(),
      matcher.condition_factory(),
      *base::test::ParseJson(
           "{\n"
           "  \"pageUrl\": {\"hostSuffix\": \"example.com\"},\n"
           "  \"css\": [\"input\"],\n"
           "  \"isBookmarked\": true,\n"
           "  \"instanceType\": \"declarativeContent.PageStateMatcher\",\n"
           "}"),
      &error);
  ASSERT_TRUE(condition);
  EXPECT_TRUE(condition->page_url_predicate);
  EXPECT_TRUE(condition->css_predicate);
  EXPECT_TRUE(condition->is_bookmarked_predicate);
}

TEST(DeclarativeContentConditionTest, WrongPageUrlDatatype) {
  URLMatcher matcher;
  std::string error;
  scoped_ptr<DeclarativeContentPageUrlPredicate> predicate =
      CreatePageUrlPredicate(*base::test::ParseJson("[]"),
                             matcher.condition_factory(), &error);
  EXPECT_THAT(error, HasSubstr("invalid type"));
  EXPECT_FALSE(predicate);

  EXPECT_TRUE(matcher.IsEmpty()) << "Errors shouldn't add URL conditions";
}

TEST(DeclarativeContentConditionTest, PageUrlPredicate) {
  URLMatcher matcher;
  scoped_refptr<Extension> extension =
      CreateExtensionWithBookmarksPermission(false);

  std::string error;
  scoped_ptr<DeclarativeContentPageUrlPredicate> predicate =
      CreatePageUrlPredicate(
          *base::test::ParseJson("{\"hostSuffix\": \"example.com\"}"),
          matcher.condition_factory(),
          &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(predicate);

  URLMatcherConditionSet::Vector all_new_condition_sets;
  all_new_condition_sets.push_back(predicate->url_matcher_condition_set());
  matcher.AddConditionSets(all_new_condition_sets);
  EXPECT_FALSE(matcher.IsEmpty());

  EXPECT_THAT(matcher.MatchURL(GURL("http://google.com/")),
              ElementsAre(/*empty*/));
  std::set<url_matcher::URLMatcherConditionSet::ID> page_url_matches =
      matcher.MatchURL(GURL("http://www.example.com/foobar"));
  EXPECT_THAT(
      page_url_matches,
      ElementsAre(predicate->url_matcher_condition_set()->id()));

  EXPECT_TRUE(predicate->Evaluate(page_url_matches));
}

TEST(DeclarativeContentConditionTest, WrongCssDatatype) {
  std::string error;
  scoped_ptr<DeclarativeContentCssPredicate> predicate = CreateCssPredicate(
      *base::test::ParseJson("\"selector\""),
      &error);
  EXPECT_THAT(error, HasSubstr("invalid type"));
  EXPECT_FALSE(predicate);
}

TEST(DeclarativeContentConditionTest, CssPredicate) {
  std::string error;
  scoped_ptr<DeclarativeContentCssPredicate> predicate = CreateCssPredicate(
      *base::test::ParseJson("[\"input\"]"),
      &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(predicate);

  base::hash_set<std::string> matched_css_selectors;
  matched_css_selectors.insert("input");

  EXPECT_TRUE(predicate->Evaluate(matched_css_selectors));

  matched_css_selectors.clear();
  matched_css_selectors.insert("body");
  EXPECT_FALSE(predicate->Evaluate(matched_css_selectors));
}

// Tests that condition with isBookmarked requires "bookmarks" permission.
TEST(DeclarativeContentConditionTest, IsBookmarkedRequiresBookmarkPermission) {
  scoped_refptr<Extension> extension =
      CreateExtensionWithBookmarksPermission(false);
  std::string error;
  scoped_ptr<DeclarativeContentIsBookmarkedPredicate> predicate =
      CreateIsBookmarkedPredicate(*base::test::ParseJson("true"),
                                  extension.get(),
                                  &error);
  EXPECT_THAT(error, HasSubstr("requires 'bookmarks' permission"));
  EXPECT_FALSE(predicate);
}

// Tests an invalid isBookmarked value type.
TEST(DeclarativeContentConditionTest, WrongIsBookmarkedDatatype) {
  scoped_refptr<Extension> extension =
      CreateExtensionWithBookmarksPermission(true);
  std::string error;
  scoped_ptr<DeclarativeContentIsBookmarkedPredicate> predicate =
      CreateIsBookmarkedPredicate(*base::test::ParseJson("[]"),
                                  extension.get(),
                                  &error);
  EXPECT_THAT(error, HasSubstr("invalid type"));
  EXPECT_FALSE(predicate);
}

// Tests isBookmark: true.
TEST(DeclarativeContentConditionTest, IsBookmarkedTrue) {
  scoped_refptr<Extension> extension =
      CreateExtensionWithBookmarksPermission(true);
  std::string error;
  scoped_ptr<DeclarativeContentIsBookmarkedPredicate> predicate =
      CreateIsBookmarkedPredicate(*base::test::ParseJson("true"),
                                  extension.get(),
                                  &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(predicate);

  EXPECT_TRUE(predicate->Evaluate(true /* url_is_bookmarked */));
  EXPECT_FALSE(predicate->Evaluate(false /* url_is_bookmarked */));
}

// Tests isBookmark: false.
TEST(DeclarativeContentConditionTest, IsBookmarkedFalse) {
  scoped_refptr<Extension> extension =
      CreateExtensionWithBookmarksPermission(true);
  std::string error;
  scoped_ptr<DeclarativeContentIsBookmarkedPredicate> predicate =
      CreateIsBookmarkedPredicate(*base::test::ParseJson("false"),
                                  extension.get(),
                                  &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(predicate);

  EXPECT_FALSE(predicate->Evaluate(true /* url_is_bookmarked */));
  EXPECT_TRUE(predicate->Evaluate(false /* url_is_bookmarked */));
}

}  // namespace extensions
