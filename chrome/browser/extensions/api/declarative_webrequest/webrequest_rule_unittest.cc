// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_rule.h"

#include "base/json/json_reader.h"
#include "base/memory/linked_ptr.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_action.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_condition.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_condition_attribute.h"
#include "chrome/common/extensions/matcher/url_matcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {
const char kExtId[] = "ext1";
}  // namespace

TEST(WebRequestRuleTest, Create) {
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
      "      \"instanceType\": \"declarativeWebRequest.CancelRequest\"   \n"
      "    }                                                             \n"
      "  ],                                                              \n"
      "  \"priority\": 200                                               \n"
      "}                                                                 ";

  scoped_ptr<Value> value(base::JSONReader::Read(kRule));
  ASSERT_TRUE(value.get());

  linked_ptr<RulesRegistry::Rule> rule(new RulesRegistry::Rule);
  ASSERT_TRUE(RulesRegistry::Rule::Populate(*value, rule.get()));

  URLMatcher matcher;
  std::string error;
  scoped_ptr<WebRequestRule> web_request_rule(
      WebRequestRule::Create(matcher.condition_factory(), kExtId, base::Time(),
                             rule, &error));
  ASSERT_TRUE(web_request_rule.get());
  EXPECT_TRUE(error.empty());

  EXPECT_EQ(kExtId, web_request_rule->id().first);
  EXPECT_EQ("rule1", web_request_rule->id().second);

  EXPECT_EQ(200, web_request_rule->priority());

  const WebRequestConditionSet& condition_set = web_request_rule->conditions();
  const WebRequestConditionSet::Conditions conditions =
      condition_set.conditions();
  ASSERT_EQ(1u, conditions.size());
  linked_ptr<WebRequestCondition> condition = conditions[0];
  const WebRequestConditionAttributes& condition_attributes =
      condition->condition_attributes();
  ASSERT_EQ(1u, condition_attributes.size());
  EXPECT_EQ(WebRequestConditionAttribute::CONDITION_CONTENT_TYPE,
            condition_attributes[0]->GetType());

  const WebRequestActionSet& action_set = web_request_rule->actions();
  const WebRequestActionSet::Actions& actions = action_set.actions();
  ASSERT_EQ(1u, actions.size());
  EXPECT_EQ(WebRequestAction::ACTION_CANCEL_REQUEST, actions[0]->GetType());
}

TEST(WebRequestRuleTest, CheckConsistency) {
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

  linked_ptr<RulesRegistry::Rule> rule(new RulesRegistry::Rule);
  ASSERT_TRUE(RulesRegistry::Rule::Populate(*value, rule.get()));

  URLMatcher matcher;
  std::string error;
  scoped_ptr<WebRequestRule> web_request_rule(
      WebRequestRule::Create(matcher.condition_factory(), kExtId, base::Time(),
                             rule, &error));
  EXPECT_FALSE(web_request_rule.get());
  EXPECT_FALSE(error.empty());
}

}  // namespace extensions
