// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/chrome_content_rules_registry.h"

#include <string>

#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using base::test::ParseJson;
using testing::HasSubstr;
using content::WebContents;

// Must be outside the anonymous namespace to be a friend of
// ContentRulesRegistry.
class DeclarativeChromeContentRulesRegistryTest : public testing::Test {
 protected:
  static const std::map<int, std::set<ContentRule*> >& active_rules(
      const ChromeContentRulesRegistry& registry) {
    return registry.active_rules_;
  }
};

namespace {

TEST_F(DeclarativeChromeContentRulesRegistryTest, ActiveRulesDoesntGrow) {
  TestExtensionEnvironment env;

  scoped_refptr<ChromeContentRulesRegistry> registry(
      new ChromeContentRulesRegistry(env.profile(), NULL));

  EXPECT_EQ(0u, active_rules(*registry.get()).size());

  content::LoadCommittedDetails load_details;
  content::FrameNavigateParams navigate_params;
  scoped_ptr<WebContents> tab = env.MakeTab();
  registry->DidNavigateMainFrame(tab.get(), load_details, navigate_params);
  EXPECT_EQ(0u, active_rules(*registry.get()).size());

  // Add a rule.
  linked_ptr<RulesRegistry::Rule> rule(new RulesRegistry::Rule);
  RulesRegistry::Rule::Populate(
      *ParseJson(
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
  std::vector<linked_ptr<RulesRegistry::Rule> > rules;
  rules.push_back(rule);

  const Extension* extension = env.MakeExtension(*ParseJson(
      "{\"page_action\": {}}"));
  registry->AddRulesImpl(extension->id(), rules);

  registry->DidNavigateMainFrame(tab.get(), load_details, navigate_params);
  EXPECT_EQ(0u, active_rules(*registry.get()).size());

  std::vector<std::string> css_selectors;
  css_selectors.push_back("input");
  registry->Apply(tab.get(), css_selectors);
  EXPECT_EQ(1u, active_rules(*registry.get()).size());

  // Closing the tab should erase its entry from active_rules_.
  tab.reset();
  EXPECT_EQ(0u, active_rules(*registry.get()).size());

  tab = env.MakeTab();
  registry->Apply(tab.get(), css_selectors);
  EXPECT_EQ(1u, active_rules(*registry.get()).size());
  // Navigating the tab should erase its entry from active_rules_ if
  // it no longer matches.
  registry->DidNavigateMainFrame(tab.get(), load_details, navigate_params);
  EXPECT_EQ(0u, active_rules(*registry.get()).size());
}

}  // namespace
}  // namespace extensions
