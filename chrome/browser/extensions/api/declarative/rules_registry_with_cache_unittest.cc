// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative/test_rules_registry.h"

// Here we test the TestRulesRegistry which is the simplest possible
// implementation of RulesRegistryWithCache as a proxy for
// RulesRegistryWithCache.

#include "base/message_loop.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char extension_id[] = "ext";
const char extension2_id[] = "ext2";
const char rule_id[] = "rule";
const char rule2_id[] = "rule2";
}

namespace extensions {

class RulesRegistryWithCacheTest : public testing::Test {
 public:
  RulesRegistryWithCacheTest()
      : ui_(content::BrowserThread::UI, &message_loop_),
        registry_(new TestRulesRegistry()) {}

  virtual ~RulesRegistryWithCacheTest() {}

  virtual void TearDown() OVERRIDE {
    // Make sure that deletion traits of all registries are executed.
    registry_ = NULL;
    message_loop_.RunUntilIdle();
  }

  std::string AddRule(const std::string& extension_id,
                      const std::string rule_id) {
    std::vector<linked_ptr<extensions::RulesRegistry::Rule> > add_rules;
    add_rules.push_back(make_linked_ptr(new extensions::RulesRegistry::Rule));
    add_rules[0]->id.reset(new std::string(rule_id));
    return registry_->AddRules(extension_id, add_rules);
  }

  std::string RemoveRule(const std::string& extension_id,
                         const std::string rule_id) {
    std::vector<std::string> remove_rules;
    remove_rules.push_back(rule_id);
    return registry_->RemoveRules(extension_id, remove_rules);
  }

  int GetNumberOfRules(const std::string& extension_id) {
    std::vector<linked_ptr<extensions::RulesRegistry::Rule> > get_rules;
    std::string error = registry_->GetAllRules(extension_id, &get_rules);
    EXPECT_EQ("", error);
    return get_rules.size();
  }

 protected:
  MessageLoop message_loop_;
  content::TestBrowserThread ui_;
  scoped_refptr<TestRulesRegistry> registry_;
};

TEST_F(RulesRegistryWithCacheTest, AddRules) {
  // Check that nothing happens if the concrete RulesRegistry refuses to insert
  // the rules.
  registry_->SetResult("Error");
  EXPECT_EQ("Error", AddRule(extension_id, rule_id));
  EXPECT_EQ(0, GetNumberOfRules(extension_id));
  registry_->SetResult("");

  // Check that rules can be inserted.
  EXPECT_EQ("", AddRule(extension_id, rule_id));
  EXPECT_EQ(1, GetNumberOfRules(extension_id));

  // Check that rules cannot be inserted twice with the same rule_id.
  EXPECT_NE("", AddRule(extension_id, rule_id));
  EXPECT_EQ(1, GetNumberOfRules(extension_id));

  // Check that different extensions may use the same rule_id.
  EXPECT_EQ("", AddRule(extension2_id, rule_id));
  EXPECT_EQ(1, GetNumberOfRules(extension_id));
  EXPECT_EQ(1, GetNumberOfRules(extension2_id));
}

TEST_F(RulesRegistryWithCacheTest, RemoveRules) {
  // Prime registry.
  EXPECT_EQ("", AddRule(extension_id, rule_id));
  EXPECT_EQ("", AddRule(extension2_id, rule_id));
  EXPECT_EQ(1, GetNumberOfRules(extension_id));
  EXPECT_EQ(1, GetNumberOfRules(extension2_id));

  // Check that nothing happens if the concrete RuleRegistry refuses to remove
  // the rules.
  registry_->SetResult("Error");
  EXPECT_EQ("Error", RemoveRule(extension_id, rule_id));
  EXPECT_EQ(1, GetNumberOfRules(extension_id));
  registry_->SetResult("");

  // Check that nothing happens if a rule does not exist.
  EXPECT_EQ("", RemoveRule(extension_id, "unknown_rule"));
  EXPECT_EQ(1, GetNumberOfRules(extension_id));

  // Check that rules may be removed and only for the correct extension.
  EXPECT_EQ("", RemoveRule(extension_id, rule_id));
  EXPECT_EQ(0, GetNumberOfRules(extension_id));
  EXPECT_EQ(1, GetNumberOfRules(extension2_id));
}

TEST_F(RulesRegistryWithCacheTest, RemoveAllRules) {
  // Prime registry.
  EXPECT_EQ("", AddRule(extension_id, rule_id));
  EXPECT_EQ("", AddRule(extension_id, rule2_id));
  EXPECT_EQ("", AddRule(extension2_id, rule_id));
  EXPECT_EQ(2, GetNumberOfRules(extension_id));
  EXPECT_EQ(1, GetNumberOfRules(extension2_id));

  // Check that nothing happens if the concrete RuleRegistry refuses to remove
  // the rules.
  registry_->SetResult("Error");
  EXPECT_EQ("Error", registry_->RemoveAllRules(extension_id));
  EXPECT_EQ(2, GetNumberOfRules(extension_id));
  registry_->SetResult("");

  // Check that rules may be removed and only for the correct extension.
  EXPECT_EQ("", registry_->RemoveAllRules(extension_id));
  EXPECT_EQ(0, GetNumberOfRules(extension_id));
  EXPECT_EQ(1, GetNumberOfRules(extension2_id));
}

TEST_F(RulesRegistryWithCacheTest, GetRules) {
  // Prime registry.
  EXPECT_EQ("", AddRule(extension_id, rule_id));
  EXPECT_EQ("", AddRule(extension_id, rule2_id));
  EXPECT_EQ("", AddRule(extension2_id, rule_id));

  // Check that we get the correct rule and unknown rules are ignored.
  std::vector<std::string> rules_to_get;
  rules_to_get.push_back(rule_id);
  rules_to_get.push_back("unknown_rule");
  std::vector<linked_ptr<extensions::RulesRegistry::Rule> > gotten_rules;
  EXPECT_EQ("", registry_->GetRules(extension_id, rules_to_get, &gotten_rules));
  ASSERT_EQ(1u, gotten_rules.size());
  ASSERT_TRUE(gotten_rules[0]->id.get());
  EXPECT_EQ(rule_id, *(gotten_rules[0]->id));
}

TEST_F(RulesRegistryWithCacheTest, GetAllRules) {
  // Prime registry.
  EXPECT_EQ("", AddRule(extension_id, rule_id));
  EXPECT_EQ("", AddRule(extension_id, rule2_id));
  EXPECT_EQ("", AddRule(extension2_id, rule_id));

  // Check that we get the correct rules.
  std::vector<linked_ptr<extensions::RulesRegistry::Rule> > gotten_rules;
  EXPECT_EQ("", registry_->GetAllRules(extension_id, &gotten_rules));
  EXPECT_EQ(2u, gotten_rules.size());
  ASSERT_TRUE(gotten_rules[0]->id.get());
  ASSERT_TRUE(gotten_rules[1]->id.get());
  EXPECT_TRUE( (rule_id == *(gotten_rules[0]->id) &&
                rule2_id == *(gotten_rules[1]->id)) ||
               (rule_id == *(gotten_rules[1]->id) &&
                rule2_id == *(gotten_rules[0]->id)) );
}

TEST_F(RulesRegistryWithCacheTest, OnExtensionUnloaded) {
  // Prime registry.
  EXPECT_EQ("", AddRule(extension_id, rule_id));
  EXPECT_EQ("", AddRule(extension2_id, rule_id));

  // Check that the correct rules are removed.
  registry_->OnExtensionUnloaded(extension_id);
  EXPECT_EQ(0, GetNumberOfRules(extension_id));
  EXPECT_EQ(1, GetNumberOfRules(extension2_id));
}

}  //  namespace extensions
