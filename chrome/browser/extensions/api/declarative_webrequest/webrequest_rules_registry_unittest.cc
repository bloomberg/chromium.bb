// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_rules_registry.h"

#include <vector>

#include "base/json/json_reader.h"
#include "base/memory/linked_ptr.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_constants.h"
#include "chrome/browser/extensions/api/web_request/web_request_api_helpers.h"
#include "chrome/common/extensions/matcher/url_matcher_constants.h"
#include "content/public/test/test_browser_thread.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kExtensionId[] = "ext1";
const char kExtensionId2[] = "ext2";
const char kRuleId1[] = "rule1";
const char kRuleId2[] = "rule2";
const char kRuleId3[] = "rule3";
const char kRuleId4[] = "rule4";
}  // namespace

namespace extensions {

using testing::HasSubstr;

namespace helpers = extension_web_request_api_helpers;
namespace keys = declarative_webrequest_constants;
namespace keys2 = url_matcher_constants;

class TestWebRequestRulesRegistry : public WebRequestRulesRegistry {
 public:
  TestWebRequestRulesRegistry()
      : WebRequestRulesRegistry(NULL, NULL),
        num_clear_cache_calls_(0) {}

  // Returns how often the in-memory caches of the renderers were instructed
  // to be cleared.
  int num_clear_cache_calls() const { return num_clear_cache_calls_; }

  // How many rules are there which have some conditions not triggered by URL
  // matches.
  size_t RulesWithoutTriggers() const {
    return rules_with_untriggered_conditions_for_test().size();
  }

 protected:
  virtual ~TestWebRequestRulesRegistry() {}

  virtual base::Time GetExtensionInstallationTime(
      const std::string& extension_id) const {
    if (extension_id == kExtensionId)
      return base::Time() + base::TimeDelta::FromDays(1);
    else if (extension_id == kExtensionId2)
      return base::Time() + base::TimeDelta::FromDays(2);
    else
      return base::Time();
  }

  virtual void ClearCacheOnNavigation() {
    ++num_clear_cache_calls_;
  }

 private:
  int num_clear_cache_calls_;
};

class WebRequestRulesRegistryTest : public testing::Test {
 public:
  WebRequestRulesRegistryTest()
      : message_loop(MessageLoop::TYPE_IO),
        ui(content::BrowserThread::UI, &message_loop),
        io(content::BrowserThread::IO, &message_loop) {}

  virtual ~WebRequestRulesRegistryTest() {}

  virtual void TearDown() OVERRIDE {
    // Make sure that deletion traits of all registries are executed.
    message_loop.RunUntilIdle();
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

    linked_ptr<json_schema_compiler::any::Any> condition1(
        new json_schema_compiler::any::Any);
    condition1->Init(http_condition_url_filter);

    linked_ptr<json_schema_compiler::any::Any> condition2(
        new json_schema_compiler::any::Any);
    condition2->Init(https_condition_url_filter);

    DictionaryValue action_dict;
    action_dict.SetString(keys::kInstanceTypeKey, keys::kCancelRequestType);

    linked_ptr<json_schema_compiler::any::Any> action1(
        new json_schema_compiler::any::Any);
    action1->Init(action_dict);

    linked_ptr<RulesRegistry::Rule> rule(new RulesRegistry::Rule);
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

    linked_ptr<json_schema_compiler::any::Any> condition(
        new json_schema_compiler::any::Any);
    condition->Init(condition_dict);

    DictionaryValue action_dict;
    action_dict.SetString(keys::kInstanceTypeKey, keys::kCancelRequestType);

    linked_ptr<json_schema_compiler::any::Any> action(
        new json_schema_compiler::any::Any);
    action->Init(action_dict);

    linked_ptr<RulesRegistry::Rule> rule(new RulesRegistry::Rule);
    rule->id.reset(new std::string(kRuleId2));
    rule->priority.reset(new int(100));
    rule->actions.push_back(action);
    rule->conditions.push_back(condition);
    return rule;
  }

  linked_ptr<RulesRegistry::Rule> CreateRedirectRule(
      const std::string& destination) {
    DictionaryValue condition_dict;
    condition_dict.SetString(keys::kInstanceTypeKey, keys::kRequestMatcherType);

    linked_ptr<json_schema_compiler::any::Any> condition(
        new json_schema_compiler::any::Any);
    condition->Init(condition_dict);

    DictionaryValue action_dict;
    action_dict.SetString(keys::kInstanceTypeKey, keys::kRedirectRequestType);
    action_dict.SetString(keys::kRedirectUrlKey, destination);

    linked_ptr<json_schema_compiler::any::Any> action(
        new json_schema_compiler::any::Any);
    action->Init(action_dict);

    linked_ptr<RulesRegistry::Rule> rule(new RulesRegistry::Rule);
    rule->id.reset(new std::string(kRuleId3));
    rule->priority.reset(new int(100));
    rule->actions.push_back(action);
    rule->conditions.push_back(condition);
    return rule;
  }

  // Create a rule to ignore all other rules for a destination that
  // contains index.html.
  linked_ptr<RulesRegistry::Rule> CreateIgnoreRule() {
    linked_ptr<json_schema_compiler::any::Any> condition(
        new json_schema_compiler::any::Any);
    DictionaryValue condition_dict;
    DictionaryValue* http_condition_dict = new DictionaryValue();
    http_condition_dict->SetString(keys2::kPathContainsKey, "index.html");
    condition_dict.SetString(keys::kInstanceTypeKey, keys::kRequestMatcherType);
    condition_dict.Set(keys::kUrlKey, http_condition_dict);
    condition->Init(condition_dict);

    DictionaryValue action_dict;
    action_dict.SetString(keys::kInstanceTypeKey, keys::kIgnoreRulesType);
    action_dict.SetInteger(keys::kLowerPriorityThanKey, 150);
    linked_ptr<json_schema_compiler::any::Any> action(
        new json_schema_compiler::any::Any);
    action->Init(action_dict);

    linked_ptr<RulesRegistry::Rule> rule(new RulesRegistry::Rule);
    rule->id.reset(new std::string(kRuleId4));
    rule->priority.reset(new int(200));
    rule->actions.push_back(action);
    rule->conditions.push_back(condition);
    return rule;
  }

  // Create a condition with the attributes specified. An example value of a
  // string from |attributes| is: "\"resourceType\": [\"stylesheet\"], \n".
  linked_ptr<json_schema_compiler::any::Any> CreateCondition(
      const std::vector<const char *>& attributes) {
    linked_ptr<json_schema_compiler::any::Any> condition(
        new json_schema_compiler::any::Any);

    // Starting boilerplate.
    std::string json_description =
        "{ \n"
        "  \"instanceType\": \"declarativeWebRequest.RequestMatcher\", \n";
    for (size_t i = 0; i < attributes.size(); ++i) {
      json_description += attributes[i];
    }
    // Ending boilerplate.
    json_description += "}";
    condition->Init(*base::test::ParseJson(json_description));

    return condition;
  }

  // Create a rule with the ID |rule_id| and with a single condition and a
  // cancelling action. The condition contains a non-matching non-URL attribute,
  // and optionally, according to |with_url_attribute| also a URL attribute
  // matching any (!) URL.
  linked_ptr<RulesRegistry::Rule> CreateNonMatchingRule(
      bool with_url_attribute,
      const char* rule_id) {
    std::vector<const char*> attributes;
    if (with_url_attribute)
      attributes.push_back("\"url\": { \"pathContains\": \"\" }, \n");

    // The following attribute never matches in this unit test.
    attributes.push_back("\"resourceType\": [\"stylesheet\"], \n");

    linked_ptr<json_schema_compiler::any::Any> condition =
        CreateCondition(attributes);

    DictionaryValue action_dict;
    action_dict.SetString(keys::kInstanceTypeKey, keys::kCancelRequestType);
    linked_ptr<json_schema_compiler::any::Any> action(
        new json_schema_compiler::any::Any);
    action->Init(action_dict);

    linked_ptr<RulesRegistry::Rule> rule(new RulesRegistry::Rule);
    rule->id.reset(new std::string(rule_id));
    rule->priority.reset(new int(1));
    rule->actions.push_back(action);
    rule->conditions.push_back(condition);
    return rule;
  }

  // Create a rule with the ID |rule_id| and with two conditions and a
  // cancelling action. One condition contains a non-matching non-URL attribute,
  // and the other one a URL attribute matching any URL.
  linked_ptr<RulesRegistry::Rule> CreateRuleWithTwoConditions(
      const char* rule_id) {
    std::vector<const char*> attributes;
    attributes.push_back("\"url\": { \"pathContains\": \"\" }, \n");
    linked_ptr<json_schema_compiler::any::Any> url_condition =
        CreateCondition(attributes);

    attributes.clear();
    // The following attribute never matches in this unit test.
    attributes.push_back("\"resourceType\": [\"stylesheet\"], \n");
    linked_ptr<json_schema_compiler::any::Any> non_matching_condition =
        CreateCondition(attributes);

    DictionaryValue action_dict;
    action_dict.SetString(keys::kInstanceTypeKey, keys::kCancelRequestType);
    linked_ptr<json_schema_compiler::any::Any> action(
        new json_schema_compiler::any::Any);
    action->Init(action_dict);

    linked_ptr<RulesRegistry::Rule> rule(new RulesRegistry::Rule);
    rule->id.reset(new std::string(rule_id));
    rule->priority.reset(new int(1));
    rule->actions.push_back(action);
    rule->conditions.push_back(url_condition);
    rule->conditions.push_back(non_matching_condition);
    return rule;
  }

 protected:
  MessageLoop message_loop;
  content::TestBrowserThread ui;
  content::TestBrowserThread io;
};

TEST_F(WebRequestRulesRegistryTest, AddRulesImpl) {
  scoped_refptr<TestWebRequestRulesRegistry> registry(
      new TestWebRequestRulesRegistry());
  std::string error;

  std::vector<linked_ptr<RulesRegistry::Rule> > rules;
  rules.push_back(CreateRule1());
  rules.push_back(CreateRule2());

  error = registry->AddRules(kExtensionId, rules);
  EXPECT_EQ("", error);
  EXPECT_EQ(1, registry->num_clear_cache_calls());

  std::set<const WebRequestRule*> matches;

  GURL http_url("http://www.example.com");
  net::TestURLRequestContext context;
  net::TestURLRequest http_request(http_url, NULL, &context);
  matches = registry->GetMatches(
      DeclarativeWebRequestData(&http_request, ON_BEFORE_REQUEST));
  EXPECT_EQ(2u, matches.size());

  std::set<WebRequestRule::GlobalRuleId> matches_ids;
  for (std::set<const WebRequestRule*>::const_iterator it = matches.begin();
       it != matches.end(); ++it)
    matches_ids.insert((*it)->id());
  EXPECT_TRUE(ContainsKey(matches_ids, std::make_pair(kExtensionId, kRuleId1)));
  EXPECT_TRUE(ContainsKey(matches_ids, std::make_pair(kExtensionId, kRuleId2)));

  GURL foobar_url("http://www.foobar.com");
  net::TestURLRequest foobar_request(foobar_url, NULL, &context);
  matches = registry->GetMatches(
      DeclarativeWebRequestData(&foobar_request, ON_BEFORE_REQUEST));
  EXPECT_EQ(1u, matches.size());
  WebRequestRule::GlobalRuleId expected_pair =
      std::make_pair(kExtensionId, kRuleId2);
  EXPECT_EQ(expected_pair, (*matches.begin())->id());
}

TEST_F(WebRequestRulesRegistryTest, RemoveRulesImpl) {
  scoped_refptr<TestWebRequestRulesRegistry> registry(
      new TestWebRequestRulesRegistry());
  std::string error;

  // Setup RulesRegistry to contain two rules.
  std::vector<linked_ptr<RulesRegistry::Rule> > rules_to_add;
  rules_to_add.push_back(CreateRule1());
  rules_to_add.push_back(CreateRule2());
  error = registry->AddRules(kExtensionId, rules_to_add);
  EXPECT_EQ("", error);
  EXPECT_EQ(1, registry->num_clear_cache_calls());

  // Verify initial state.
  std::vector<linked_ptr<RulesRegistry::Rule> > registered_rules;
  registry->GetAllRules(kExtensionId, &registered_rules);
  EXPECT_EQ(2u, registered_rules.size());
  EXPECT_EQ(1u, registry->RulesWithoutTriggers());

  // Remove first rule.
  std::vector<std::string> rules_to_remove;
  rules_to_remove.push_back(kRuleId1);
  error = registry->RemoveRules(kExtensionId, rules_to_remove);
  EXPECT_EQ("", error);
  EXPECT_EQ(2, registry->num_clear_cache_calls());

  // Verify that only one rule is left.
  registered_rules.clear();
  registry->GetAllRules(kExtensionId, &registered_rules);
  EXPECT_EQ(1u, registered_rules.size());
  EXPECT_EQ(1u, registry->RulesWithoutTriggers());

  // Now rules_to_remove contains both rules, i.e. one that does not exist in
  // the rules registry anymore. Effectively we only remove the second rule.
  rules_to_remove.push_back(kRuleId2);
  error = registry->RemoveRules(kExtensionId, rules_to_remove);
  EXPECT_EQ("", error);
  EXPECT_EQ(3, registry->num_clear_cache_calls());

  // Verify that everything is gone.
  registered_rules.clear();
  registry->GetAllRules(kExtensionId, &registered_rules);
  EXPECT_EQ(0u, registered_rules.size());
  EXPECT_EQ(0u, registry->RulesWithoutTriggers());

  EXPECT_TRUE(registry->IsEmpty());
}

TEST_F(WebRequestRulesRegistryTest, RemoveAllRulesImpl) {
  scoped_refptr<TestWebRequestRulesRegistry> registry(
      new TestWebRequestRulesRegistry());
  std::string error;

  // Setup RulesRegistry to contain two rules, one for each extension.
  std::vector<linked_ptr<RulesRegistry::Rule> > rules_to_add(1);
  rules_to_add[0] = CreateRule1();
  error = registry->AddRules(kExtensionId, rules_to_add);
  EXPECT_EQ("", error);
  EXPECT_EQ(1, registry->num_clear_cache_calls());

  rules_to_add[0] = CreateRule2();
  error = registry->AddRules(kExtensionId2, rules_to_add);
  EXPECT_EQ("", error);
  EXPECT_EQ(2, registry->num_clear_cache_calls());

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
  EXPECT_EQ(3, registry->num_clear_cache_calls());

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
  EXPECT_EQ(4, registry->num_clear_cache_calls());

  // Remove rule from second extension.
  error = registry->RemoveAllRules(kExtensionId2);
  EXPECT_EQ("", error);
  EXPECT_EQ(5, registry->num_clear_cache_calls());

  EXPECT_TRUE(registry->IsEmpty());
}

// Test precedences between extensions.
TEST_F(WebRequestRulesRegistryTest, Precedences) {
  scoped_refptr<WebRequestRulesRegistry> registry(
      new TestWebRequestRulesRegistry());
  std::string error;

  std::vector<linked_ptr<RulesRegistry::Rule> > rules_to_add_1(1);
  rules_to_add_1[0] = CreateRedirectRule("http://www.foo.com");
  error = registry->AddRules(kExtensionId, rules_to_add_1);
  EXPECT_EQ("", error);

  std::vector<linked_ptr<RulesRegistry::Rule> > rules_to_add_2(1);
  rules_to_add_2[0] = CreateRedirectRule("http://www.bar.com");
  error = registry->AddRules(kExtensionId2, rules_to_add_2);
  EXPECT_EQ("", error);

  GURL url("http://www.google.com");
  net::TestURLRequestContext context;
  net::TestURLRequest request(url, NULL, &context);
  std::list<LinkedPtrEventResponseDelta> deltas =
      registry->CreateDeltas(
          NULL,
          DeclarativeWebRequestData(&request, ON_BEFORE_REQUEST),
          false);

  // The second extension is installed later and will win for this reason
  // in conflict resolution.
  ASSERT_EQ(2u, deltas.size());
  deltas.sort(&helpers::InDecreasingExtensionInstallationTimeOrder);

  std::list<LinkedPtrEventResponseDelta>::iterator i = deltas.begin();
  LinkedPtrEventResponseDelta winner = *i++;
  LinkedPtrEventResponseDelta loser = *i;

  EXPECT_EQ(kExtensionId2, winner->extension_id);
  EXPECT_EQ(base::Time() + base::TimeDelta::FromDays(2),
            winner->extension_install_time);
  EXPECT_EQ(GURL("http://www.bar.com"), winner->new_url);

  EXPECT_EQ(kExtensionId, loser->extension_id);
  EXPECT_EQ(base::Time() + base::TimeDelta::FromDays(1),
            loser->extension_install_time);
  EXPECT_EQ(GURL("http://www.foo.com"), loser->new_url);
}

// Test priorities of rules within one extension.
TEST_F(WebRequestRulesRegistryTest, Priorities) {
  scoped_refptr<WebRequestRulesRegistry> registry(
      new TestWebRequestRulesRegistry());
  std::string error;

  std::vector<linked_ptr<RulesRegistry::Rule> > rules_to_add_1(1);
  rules_to_add_1[0] = CreateRedirectRule("http://www.foo.com");
  error = registry->AddRules(kExtensionId, rules_to_add_1);
  EXPECT_EQ("", error);

  std::vector<linked_ptr<RulesRegistry::Rule> > rules_to_add_2(1);
  rules_to_add_2[0] = CreateRedirectRule("http://www.bar.com");
  error = registry->AddRules(kExtensionId2, rules_to_add_2);
  EXPECT_EQ("", error);

  std::vector<linked_ptr<RulesRegistry::Rule> > rules_to_add_3(1);
  rules_to_add_3[0] = CreateIgnoreRule();
  error = registry->AddRules(kExtensionId, rules_to_add_3);
  EXPECT_EQ("", error);

  GURL url("http://www.google.com/index.html");
  net::TestURLRequestContext context;
  net::TestURLRequest request(url, NULL, &context);
  std::list<LinkedPtrEventResponseDelta> deltas =
      registry->CreateDeltas(
          NULL,
          DeclarativeWebRequestData(&request, ON_BEFORE_REQUEST),
          false);

  // The redirect by the first extension is ignored due to the ignore rule.
  ASSERT_EQ(1u, deltas.size());
  LinkedPtrEventResponseDelta effective_rule = *(deltas.begin());

  EXPECT_EQ(kExtensionId2, effective_rule->extension_id);
  EXPECT_EQ(base::Time() + base::TimeDelta::FromDays(2),
            effective_rule->extension_install_time);
  EXPECT_EQ(GURL("http://www.bar.com"), effective_rule->new_url);
}

// Test that rules failing IsFulfilled on their conditions are never returned by
// GetMatches.
TEST_F(WebRequestRulesRegistryTest, GetMatchesCheckFulfilled) {
  scoped_refptr<TestWebRequestRulesRegistry> registry(
      new TestWebRequestRulesRegistry());
  std::string error;

  std::vector<linked_ptr<RulesRegistry::Rule> > rules;
  // Both rules have one condition, neither of them should fire.
  // This rule's condition has only one, non-matching and non-URL attribute.
  rules.push_back(CreateNonMatchingRule(false, kRuleId1));
  // This rule's condition has two attributes: a matching URL, and a
  // non-matching non-URL attribute.
  rules.push_back(CreateNonMatchingRule(true, kRuleId2));

  // The 3rd rule has two conditions, one with a matching URL attribute, and one
  // with a non-matching non-URL attribute.
  rules.push_back(CreateRuleWithTwoConditions(kRuleId3));

  error = registry->AddRules(kExtensionId, rules);
  EXPECT_EQ("", error);
  EXPECT_EQ(1, registry->num_clear_cache_calls());

  std::set<const WebRequestRule*> matches;

  GURL http_url("http://www.example.com");
  net::TestURLRequestContext context;
  net::TestURLRequest http_request(http_url, NULL, &context);
  matches = registry->GetMatches(
      DeclarativeWebRequestData(&http_request, ON_BEFORE_REQUEST));
  EXPECT_EQ(1u, matches.size());
  WebRequestRule::GlobalRuleId expected_pair = std::make_pair(kExtensionId,
                                                              kRuleId3);
  EXPECT_EQ(expected_pair, (*matches.begin())->id());
}

TEST_F(WebRequestRulesRegistryTest, CheckConsistency) {
  // The contentType condition can only be evaluated during ON_HEADERS_RECEIVED
  // but the redirect action can only be executed during ON_BEFORE_REQUEST.
  // Therefore, this is an inconsistent rule that needs to be flagged.
  const char kRule[] =
      "{                                                                 \n"
      "  \"id\": \"rule1\",                                              \n"
      "  \"conditions\": [                                               \n"
      "    {                                                             \n"
      "      \"instanceType\": \"declarativeWebRequest.RequestMatcher\", \n"
      "      \"url\": {\"hostSuffix\": \"foo.com\"},                     \n"
      "      \"contentType\": [\"image/jpeg\"]                           \n"
      "    }                                                             \n"
      "  ],                                                              \n"
      "  \"actions\": [                                                  \n"
      "    {                                                             \n"
      "      \"instanceType\": \"declarativeWebRequest.RedirectRequest\",\n"
      "      \"redirectUrl\": \"http://bar.com\"                         \n"
      "    }                                                             \n"
      "  ],                                                              \n"
      "  \"priority\": 200                                               \n"
      "}                                                                 ";

  scoped_ptr<Value> value(base::JSONReader::Read(kRule));
  ASSERT_TRUE(value.get());

  std::vector<linked_ptr<RulesRegistry::Rule> > rules;
  rules.push_back(make_linked_ptr(new RulesRegistry::Rule));
  ASSERT_TRUE(RulesRegistry::Rule::Populate(*value, rules.back().get()));

  scoped_refptr<WebRequestRulesRegistry> registry(
      new TestWebRequestRulesRegistry());

  URLMatcher matcher;
  std::string error = registry->AddRulesImpl(kExtensionId, rules);
  EXPECT_THAT(error, HasSubstr("no time in the request life-cycle"));
  EXPECT_TRUE(registry->IsEmpty());
}


}  // namespace extensions
