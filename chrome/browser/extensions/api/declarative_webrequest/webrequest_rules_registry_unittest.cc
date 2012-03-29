// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_rules_registry.h"

#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "content/test/test_browser_thread.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kExtensionId[] = "ext1";
const char kRuleId1[] = "rule1";
const char kRuleId2[] = "rule2";

const char kCancelRequestType[] = "experimental.webRequest.CancelRequest";
const char kRequestMatcher[] = "experimental.webRequest.RequestMatcher";
const char kInstanceType[] = "instanceType";
}

namespace extensions {

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
    DictionaryValue http_condition_dict;
    http_condition_dict.SetString("scheme", "http");
    http_condition_dict.SetString("host_suffix", "example.com");
    http_condition_dict.SetString(kInstanceType, kRequestMatcher);

    DictionaryValue https_condition_dict;
    https_condition_dict.SetString("scheme", "https");
    https_condition_dict.SetString("host_suffix", "example.com");
    https_condition_dict.SetString("host_prefix", "www");
    https_condition_dict.SetString(kInstanceType, kRequestMatcher);

    linked_ptr<json_schema_compiler::any::Any> condition1 = make_linked_ptr(
        new json_schema_compiler::any::Any);
    condition1->Init(http_condition_dict);

    linked_ptr<json_schema_compiler::any::Any> condition2 = make_linked_ptr(
        new json_schema_compiler::any::Any);
    condition2->Init(https_condition_dict);

    DictionaryValue action_dict;
    action_dict.SetString(kInstanceType, kCancelRequestType);

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
    condition_dict.SetString(kInstanceType, kRequestMatcher);

    linked_ptr<json_schema_compiler::any::Any> condition = make_linked_ptr(
        new json_schema_compiler::any::Any);
    condition->Init(condition_dict);

    DictionaryValue action_dict;
    action_dict.SetString(kInstanceType, kCancelRequestType);

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
  EXPECT_TRUE(error.empty());

  std::set<WebRequestRule::GlobalRuleId> matches;

  GURL http_url("http://www.example.com");
  TestURLRequest http_request(http_url, NULL);
  matches = registry->GetMatches(&http_request);
  EXPECT_EQ(2u, matches.size());
  EXPECT_TRUE(matches.find(std::make_pair(kExtensionId, kRuleId1)) !=
      matches.end());
  EXPECT_TRUE(matches.find(std::make_pair(kExtensionId, kRuleId2)) !=
      matches.end());

  GURL foobar_url("http://www.foobar.com");
  TestURLRequest foobar_request(foobar_url, NULL);
  matches = registry->GetMatches(&foobar_request);
  EXPECT_EQ(1u, matches.size());
  EXPECT_TRUE(matches.find(std::make_pair(kExtensionId, kRuleId2)) !=
      matches.end());
}

}  // namespace extensions
