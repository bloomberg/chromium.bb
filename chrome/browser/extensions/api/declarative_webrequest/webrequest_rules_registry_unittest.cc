// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_rules_registry.h"

#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/common/extensions/matcher/url_matcher_constants.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_constants.h"
#include "content/test/test_browser_thread.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kExtensionId[] = "ext1";
const char kExtensionId2[] = "ext2";
const char kRuleId1[] = "rule1";
const char kRuleId2[] = "rule2";
}  // namespace

namespace extensions {

namespace keys = declarative_webrequest_constants;
namespace keys2 = url_matcher_constants;

class WebRequestRulesRegistryTest : public testing::Test {
 public:
 public:
  WebRequestRulesRegistryTest()
      : message_loop(MessageLoop::TYPE_IO),
        ui(content::BrowserThread::UI, &message_loop),
        io(content::BrowserThread::IO, &message_loop) {}

  virtual ~WebRequestRulesRegistryTest() {}

  virtual void TearDown() OVERRIDE {
    // Make sure that deletion traits of all registries are executed.
    message_loop.RunAllPending();
  }

  // Returns a rule that roughly matches http://*.example.com and
  // https://www.example.com and cancels it
  linked_ptr<RulesRegistry::Rule> CreateRule1() {
    ListValue* scheme_http = new ListValue();
    scheme_http->Append(Value::CreateStringValue("http"));
    DictionaryValue* http_condition_dict = new DictionaryValue();
    http_condition_dict->Set(keys2::kSchemesKey, scheme_http);
    http_condition_dict->SetString(keys2::kHostSuffixKey, "example.com");
    DictionaryValue http_condition_url_filter;
    http_condition_url_filter.Set(keys::kUrlKey, http_condition_dict);
    http_condition_url_filter.SetString(keys::kInstanceTypeKey,
                                        keys::kRequestMatcherType);

    ListValue* scheme_https = new ListValue();
    scheme_http->Append(Value::CreateStringValue("https"));
    DictionaryValue* https_condition_dict = new DictionaryValue();
    https_condition_dict->Set(keys2::kSchemesKey, scheme_https);
    https_condition_dict->SetString(keys2::kHostSuffixKey, "example.com");
    https_condition_dict->SetString(keys2::kHostPrefixKey, "www");
    DictionaryValue https_condition_url_filter;
    https_condition_url_filter.Set(keys::kUrlKey, https_condition_dict);
    https_condition_url_filter.SetString(keys::kInstanceTypeKey,
                                         keys::kRequestMatcherType);

    linked_ptr<json_schema_compiler::any::Any> condition1 = make_linked_ptr(
        new json_schema_compiler::any::Any);
    condition1->Init(http_condition_url_filter);

    linked_ptr<json_schema_compiler::any::Any> condition2 = make_linked_ptr(
        new json_schema_compiler::any::Any);
    condition2->Init(https_condition_url_filter);

    DictionaryValue action_dict;
    action_dict.SetString(keys::kInstanceTypeKey, keys::kCancelRequestType);

    linked_ptr<json_schema_compiler::any::Any> action1 = make_linked_ptr(
        new json_schema_compiler::any::Any);
    action1->Init(action_dict);

    linked_ptr<RulesRegistry::Rule> rule =
        make_linked_ptr(new RulesRegistry::Rule);
    rule->id.reset(new std::string(kRuleId1));
    rule->priority.reset(new int(100));
    rule->actions.push_back(action1);
    rule->conditions.push_back(condition1);
    rule->conditions.push_back(condition2);
    return rule;
  }

  // Returns a rule that matches anything and cancels it.
  linked_ptr<RulesRegistry::Rule> CreateRule2() {
    DictionaryValue condition_dict;
    condition_dict.SetString(keys::kInstanceTypeKey, keys::kRequestMatcherType);

    linked_ptr<json_schema_compiler::any::Any> condition = make_linked_ptr(
        new json_schema_compiler::any::Any);
    condition->Init(condition_dict);

    DictionaryValue action_dict;
    action_dict.SetString(keys::kInstanceTypeKey, keys::kCancelRequestType);

    linked_ptr<json_schema_compiler::any::Any> action = make_linked_ptr(
        new json_schema_compiler::any::Any);
    action->Init(action_dict);

    linked_ptr<RulesRegistry::Rule> rule =
        make_linked_ptr(new RulesRegistry::Rule);
    rule->id.reset(new std::string(kRuleId2));
    rule->priority.reset(new int(100));
    rule->actions.push_back(action);
    rule->conditions.push_back(condition);
    return rule;
  }

 protected:
  MessageLoop message_loop;
  content::TestBrowserThread ui;
  content::TestBrowserThread io;
};

TEST_F(WebRequestRulesRegistryTest, AddRulesImpl) {
  scoped_refptr<WebRequestRulesRegistry> registry(new WebRequestRulesRegistry);
  std::string error;

  std::vector<linked_ptr<RulesRegistry::Rule> > rules;
  rules.push_back(CreateRule1());
  rules.push_back(CreateRule2());

  error = registry->AddRules(kExtensionId, rules);
  EXPECT_EQ("", error);

  std::set<WebRequestRule::GlobalRuleId> matches;

  GURL http_url("http://www.example.com");
  TestURLRequest http_request(http_url, NULL);
  matches = registry->GetMatches(&http_request, ON_BEFORE_REQUEST);
  EXPECT_EQ(2u, matches.size());
  EXPECT_TRUE(matches.find(std::make_pair(kExtensionId, kRuleId1)) !=
      matches.end());
  EXPECT_TRUE(matches.find(std::make_pair(kExtensionId, kRuleId2)) !=
      matches.end());

  GURL foobar_url("http://www.foobar.com");
  TestURLRequest foobar_request(foobar_url, NULL);
  matches = registry->GetMatches(&foobar_request, ON_BEFORE_REQUEST);
  EXPECT_EQ(1u, matches.size());
  EXPECT_TRUE(matches.find(std::make_pair(kExtensionId, kRuleId2)) !=
      matches.end());
}

TEST_F(WebRequestRulesRegistryTest, RemoveRulesImpl) {
  scoped_refptr<WebRequestRulesRegistry> registry(new WebRequestRulesRegistry);
  std::string error;

  // Setup RulesRegistry to contain two rules.
  std::vector<linked_ptr<RulesRegistry::Rule> > rules_to_add;
  rules_to_add.push_back(CreateRule1());
  rules_to_add.push_back(CreateRule2());
  error = registry->AddRules(kExtensionId, rules_to_add);
  EXPECT_EQ("", error);

  // Verify initial state.
  std::vector<linked_ptr<RulesRegistry::Rule> > registered_rules;
  registry->GetAllRules(kExtensionId, &registered_rules);
  EXPECT_EQ(2u, registered_rules.size());

  // Remove first rule.
  std::vector<std::string> rules_to_remove;
  rules_to_remove.push_back(kRuleId1);
  error = registry->RemoveRules(kExtensionId, rules_to_remove);
  EXPECT_EQ("", error);

  // Verify that only one rule is left.
  registered_rules.clear();
  registry->GetAllRules(kExtensionId, &registered_rules);
  EXPECT_EQ(1u, registered_rules.size());

  // Now rules_to_remove contains both rules, i.e. one that does not exist in
  // the rules registry anymore. Effectively we only remove the second rule.
  rules_to_remove.push_back(kRuleId2);
  error = registry->RemoveRules(kExtensionId, rules_to_remove);
  EXPECT_EQ("", error);

  // Verify that everything is gone.
  registered_rules.clear();
  registry->GetAllRules(kExtensionId, &registered_rules);
  EXPECT_EQ(0u, registered_rules.size());

  EXPECT_TRUE(registry->IsEmpty());
}

TEST_F(WebRequestRulesRegistryTest, RemoveAllRulesImpl) {
  scoped_refptr<WebRequestRulesRegistry> registry(new WebRequestRulesRegistry);
  std::string error;

  // Setup RulesRegistry to contain two rules, one for each extension.
  std::vector<linked_ptr<RulesRegistry::Rule> > rules_to_add(1);
  rules_to_add[0] = CreateRule1();
  error = registry->AddRules(kExtensionId, rules_to_add);
  EXPECT_EQ("", error);

  rules_to_add[0] = CreateRule2();
  error = registry->AddRules(kExtensionId2, rules_to_add);
  EXPECT_EQ("", error);

  // Verify initial state.
  std::vector<linked_ptr<RulesRegistry::Rule> > registered_rules;
  registry->GetAllRules(kExtensionId, &registered_rules);
  EXPECT_EQ(1u, registered_rules.size());
  registered_rules.clear();
  registry->GetAllRules(kExtensionId2, &registered_rules);
  EXPECT_EQ(1u, registered_rules.size());

  // Remove rule of first extension.
  error = registry->RemoveAllRules(kExtensionId);
  EXPECT_EQ("", error);

  // Verify that only the first rule is deleted.
  registered_rules.clear();
  registry->GetAllRules(kExtensionId, &registered_rules);
  EXPECT_EQ(0u, registered_rules.size());
  registered_rules.clear();
  registry->GetAllRules(kExtensionId2, &registered_rules);
  EXPECT_EQ(1u, registered_rules.size());

  // Test removing rules if none exist.
  error = registry->RemoveAllRules(kExtensionId);
  EXPECT_EQ("", error);

  // Remove rule from second extension.
  error = registry->RemoveAllRules(kExtensionId2);
  EXPECT_EQ("", error);

  EXPECT_TRUE(registry->IsEmpty());
}

}  // namespace extensions
