// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/chrome_content_rules_registry.h"

#include <string>

#include "base/test/values_test_util.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

using testing::HasSubstr;

class DeclarativeChromeContentRulesRegistryTest : public testing::Test {
 public:
  DeclarativeChromeContentRulesRegistryTest() {
    env_.profile()->CreateBookmarkModel(true);
    bookmarks::test::WaitForBookmarkModelToLoad(
        BookmarkModelFactory::GetForProfile(env_.profile()));
  }

 protected:
  TestExtensionEnvironment* env() { return &env_; }

 private:
  TestExtensionEnvironment env_;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeChromeContentRulesRegistryTest);
};

TEST(DeclarativeContentConditionTest, UnknownPredicateName) {
  url_matcher::URLMatcher matcher;
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
  url_matcher::URLMatcher matcher;
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
  url_matcher::URLMatcher matcher;
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

TEST_F(DeclarativeChromeContentRulesRegistryTest, ActiveRulesDoesntGrow) {
  scoped_refptr<ChromeContentRulesRegistry> registry(
      new ChromeContentRulesRegistry(env()->profile(), NULL));

  EXPECT_EQ(0u, registry->GetActiveRulesCountForTesting());

  scoped_ptr<content::WebContents> tab = env()->MakeTab();
  registry->MonitorWebContentsForRuleEvaluation(tab.get());
  registry->DidNavigateMainFrame(tab.get(), content::LoadCommittedDetails(),
                                 content::FrameNavigateParams());
  EXPECT_EQ(0u, registry->GetActiveRulesCountForTesting());

  // Add a rule.
  linked_ptr<api::events::Rule> rule(new api::events::Rule);
  api::events::Rule::Populate(
      *base::test::ParseJson(
          "{\n"
          "  \"id\": \"rule1\",\n"
          "  \"priority\": 100,\n"
          "  \"conditions\": [\n"
          "    {\n"
          "      \"instanceType\": \"declarativeContent.PageStateMatcher\",\n"
          "      \"css\": [\"input\"]\n"
          "    }],\n"
          "  \"actions\": [\n"
          "    { \"instanceType\": \"declarativeContent.ShowPageAction\" }\n"
          "  ]\n"
          "}"),
      rule.get());
  std::vector<linked_ptr<api::events::Rule>> rules;
  rules.push_back(rule);

  const Extension* extension = env()->MakeExtension(*base::test::ParseJson(
      "{\"page_action\": {}}"));
  registry->AddRulesImpl(extension->id(), rules);

  registry->DidNavigateMainFrame(tab.get(), content::LoadCommittedDetails(),
                                 content::FrameNavigateParams());
  EXPECT_EQ(0u, registry->GetActiveRulesCountForTesting());

  std::vector<std::string> css_selectors;
  css_selectors.push_back("input");
  registry->UpdateMatchingCssSelectorsForTesting(tab.get(), css_selectors);
  EXPECT_EQ(1u, registry->GetActiveRulesCountForTesting());

  // Closing the tab should erase its entry from active_rules_.
  tab.reset();
  EXPECT_EQ(0u, registry->GetActiveRulesCountForTesting());

  tab = env()->MakeTab();
  registry->MonitorWebContentsForRuleEvaluation(tab.get());
  registry->UpdateMatchingCssSelectorsForTesting(tab.get(), css_selectors);
  EXPECT_EQ(1u, registry->GetActiveRulesCountForTesting());
  // Navigating the tab should erase its entry from active_rules_ if
  // it no longer matches.
  registry->DidNavigateMainFrame(tab.get(), content::LoadCommittedDetails(),
                                 content::FrameNavigateParams());
  EXPECT_EQ(0u, registry->GetActiveRulesCountForTesting());
}

}  // namespace extensions
