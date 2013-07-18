// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative/rules_registry_with_cache.h"

// Here we test the TestRulesRegistry which is the simplest possible
// implementation of RulesRegistryWithCache as a proxy for
// RulesRegistryWithCache.

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/extensions/api/declarative/test_rules_registry.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/value_store/testing_value_store.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

namespace {
// The |extension_id| needs to pass the Extension::IdIsValid test.
const char extension_id[] = "abcdefghijklmnopabcdefghijklmnop";
const char extension2_id[] = "ponmlkjihgfedcbaponmlkjihgfedcba";
const char rule_id[] = "rule";
const char rule2_id[] = "rule2";
}

namespace extensions {

class RulesRegistryWithCacheTest : public testing::Test {
 public:
  RulesRegistryWithCacheTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        file_thread_(content::BrowserThread::FILE, &message_loop_),
        registry_(new TestRulesRegistry(content::BrowserThread::UI,
                                        "" /*event_name*/)) {}

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
  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  scoped_refptr<TestRulesRegistry> registry_;
#if defined OS_CHROMEOS
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  chromeos::ScopedTestUserManager test_user_manager_;
#endif
};

TEST_F(RulesRegistryWithCacheTest, AddRules) {
  // Check that nothing happens if the concrete RulesRegistry refuses to insert
  // the rules.
  registry_->SetResult("Error");
  EXPECT_EQ("Error", AddRule(extension_id, rule_id));
  EXPECT_EQ(0, GetNumberOfRules(extension_id));
  registry_->SetResult(std::string());

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
  registry_->SetResult(std::string());

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
  registry_->SetResult(std::string());

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

TEST_F(RulesRegistryWithCacheTest, DeclarativeRulesStored) {
  TestingProfile profile;
  // TestingProfile::Init makes sure that the factory method for a corresponding
  // extension system creates a TestExtensionSystem.
  extensions::TestExtensionSystem* system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(&profile));
  ExtensionPrefs* extension_prefs = system->CreateExtensionPrefs(
      CommandLine::ForCurrentProcess(), base::FilePath());
  system->CreateExtensionService(
      CommandLine::ForCurrentProcess(), base::FilePath(), false);
  // The value store is first created during CreateExtensionService.
  TestingValueStore* store = system->value_store();

  scoped_ptr<RulesRegistryWithCache::RuleStorageOnUI> ui_part;
  scoped_refptr<RulesRegistryWithCache> registry(new TestRulesRegistry(
      &profile, "testEvent", content::BrowserThread::UI, &ui_part));

  // 1. Test the handling of preferences.
  // Default value is always true.
  EXPECT_TRUE(ui_part->GetDeclarativeRulesStored(extension_id));

  extension_prefs->UpdateExtensionPref(
      extension_id,
      RulesRegistryWithCache::RuleStorageOnUI::kRulesStoredKey,
      new base::FundamentalValue(false));
  EXPECT_FALSE(ui_part->GetDeclarativeRulesStored(extension_id));

  extension_prefs->UpdateExtensionPref(
      extension_id,
      RulesRegistryWithCache::RuleStorageOnUI::kRulesStoredKey,
      new base::FundamentalValue(true));
  EXPECT_TRUE(ui_part->GetDeclarativeRulesStored(extension_id));

  // 2. Test writing behavior.
  int write_count = store->write_count();

  scoped_ptr<base::ListValue> value(new base::ListValue);
  value->AppendBoolean(true);
  ui_part->WriteToStorage(extension_id, value.PassAs<base::Value>());
  EXPECT_TRUE(ui_part->GetDeclarativeRulesStored(extension_id));
  message_loop_.RunUntilIdle();
  EXPECT_EQ(write_count + 1, store->write_count());
  write_count = store->write_count();

  value.reset(new base::ListValue);
  ui_part->WriteToStorage(extension_id, value.PassAs<base::Value>());
  EXPECT_FALSE(ui_part->GetDeclarativeRulesStored(extension_id));
  message_loop_.RunUntilIdle();
  // No rules currently, but previously there were, so we expect a write.
  EXPECT_EQ(write_count + 1, store->write_count());
  write_count = store->write_count();

  value.reset(new base::ListValue);
  ui_part->WriteToStorage(extension_id, value.PassAs<base::Value>());
  EXPECT_FALSE(ui_part->GetDeclarativeRulesStored(extension_id));
  message_loop_.RunUntilIdle();
  EXPECT_EQ(write_count, store->write_count());

  // 3. Test reading behavior.
  int read_count = store->read_count();

  ui_part->SetDeclarativeRulesStored(extension_id, false);
  ui_part->ReadFromStorage(extension_id);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(read_count, store->read_count());
  read_count = store->read_count();

  ui_part->SetDeclarativeRulesStored(extension_id, true);
  ui_part->ReadFromStorage(extension_id);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(read_count + 1, store->read_count());
}

}  //  namespace extensions
