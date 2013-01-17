// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative/declarative_rule.h"

#include "base/message_loop.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/common/extensions/matcher/url_matcher_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using json_schema_compiler::any::Any;
using base::test::ParseJson;

struct RecordingCondition {
  typedef int MatchData;

  URLMatcherConditionFactory* factory;
  scoped_ptr<base::Value> value;

  void GetURLMatcherConditionSets(
      URLMatcherConditionSet::Vector* condition_sets) const {
    // No condition sets.
  }

  bool has_url_matcher_condition_set() const {
    return false;
  }

  static scoped_ptr<RecordingCondition> Create(
      URLMatcherConditionFactory* url_matcher_condition_factory,
      const base::Value& condition,
      std::string* error) {
    const base::DictionaryValue* dict = NULL;
    if (condition.GetAsDictionary(&dict) && dict->HasKey("bad_key")) {
      *error = "Found error key";
      return scoped_ptr<RecordingCondition>();
    }

    scoped_ptr<RecordingCondition> result(new RecordingCondition());
    result->factory = url_matcher_condition_factory;
    result->value.reset(condition.DeepCopy());
    return result.Pass();
  }
};
typedef DeclarativeConditionSet<RecordingCondition> RecordingConditionSet;

TEST(DeclarativeConditionTest, ErrorConditionSet) {
  URLMatcher matcher;
  RecordingConditionSet::AnyVector conditions;
  conditions.push_back(make_linked_ptr(new Any(ParseJson("{\"key\": 1}"))));
  conditions.push_back(make_linked_ptr(new Any(ParseJson("{\"bad_key\": 2}"))));

  std::string error;
  scoped_ptr<RecordingConditionSet> result =
      RecordingConditionSet::Create(matcher.condition_factory(),
                                    conditions, &error);
  EXPECT_EQ("Found error key", error);
  ASSERT_FALSE(result);
}

TEST(DeclarativeConditionTest, CreateConditionSet) {
  URLMatcher matcher;
  RecordingConditionSet::AnyVector conditions;
  conditions.push_back(make_linked_ptr(new Any(ParseJson("{\"key\": 1}"))));
  conditions.push_back(make_linked_ptr(new Any(ParseJson("[\"val1\", 2]"))));

  // Test insertion
  std::string error;
  scoped_ptr<RecordingConditionSet> result =
      RecordingConditionSet::Create(matcher.condition_factory(),
                                    conditions, &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(result);
  EXPECT_EQ(2u, result->conditions().size());

  EXPECT_EQ(matcher.condition_factory(), result->conditions()[0]->factory);
  EXPECT_TRUE(ParseJson("{\"key\": 1}")->Equals(
      result->conditions()[0]->value.get()));
}

struct FulfillableCondition {
  typedef int MatchData;

  scoped_refptr<URLMatcherConditionSet> condition_set;
  int condition_set_id;
  int max_value;

  URLMatcherConditionSet::ID url_matcher_condition_set_id() const {
    return condition_set_id;
  }

  bool has_url_matcher_condition_set() const {
    return true;
  }

  scoped_refptr<URLMatcherConditionSet> url_matcher_condition_set() const {
    return condition_set;
  }

  void GetURLMatcherConditionSets(
      URLMatcherConditionSet::Vector* condition_sets) const {
    if (condition_set)
      condition_sets->push_back(condition_set);
  }

  bool IsFulfilled(const std::set<URLMatcherConditionSet::ID>& url_matches,
                   int match_data) const {
    if (condition_set_id != -1 && !ContainsKey(url_matches, condition_set_id))
      return false;
    return match_data <= max_value;
  }

  static scoped_ptr<FulfillableCondition> Create(
      URLMatcherConditionFactory* url_matcher_condition_factory,
      const base::Value& condition,
      std::string* error) {
    scoped_ptr<FulfillableCondition> result(new FulfillableCondition());
    const base::DictionaryValue* dict;
    if (!condition.GetAsDictionary(&dict)) {
      *error = "Expected dict";
      return result.Pass();
    }
    if (!dict->GetInteger("url_id", &result->condition_set_id))
      result->condition_set_id = -1;
    if (!dict->GetInteger("max", &result->max_value))
      *error = "Expected integer at ['max']";
    if (result->condition_set_id != -1) {
      result->condition_set = new URLMatcherConditionSet(
          result->condition_set_id,
          URLMatcherConditionSet::Conditions());
    }
    return result.Pass();
  }
};

TEST(DeclarativeConditionTest, FulfillConditionSet) {
  typedef DeclarativeConditionSet<FulfillableCondition> FulfillableConditionSet;
  FulfillableConditionSet::AnyVector conditions;
  conditions.push_back(make_linked_ptr(new Any(ParseJson(
      "{\"url_id\": 1, \"max\": 3}"))));
  conditions.push_back(make_linked_ptr(new Any(ParseJson(
      "{\"url_id\": 2, \"max\": 5}"))));
  conditions.push_back(make_linked_ptr(new Any(ParseJson(
      "{\"url_id\": 3, \"max\": 1}"))));
  conditions.push_back(make_linked_ptr(new Any(ParseJson(
      "{\"max\": -5}"))));  // No url.

  // Test insertion
  std::string error;
  scoped_ptr<FulfillableConditionSet> result =
      FulfillableConditionSet::Create(NULL, conditions, &error);
  ASSERT_EQ("", error);
  ASSERT_TRUE(result);
  EXPECT_EQ(4u, result->conditions().size());

  std::set<URLMatcherConditionSet::ID> url_matches;
  EXPECT_FALSE(result->IsFulfilled(1, url_matches, 0))
      << "Testing an ID that's not in url_matches forwards to the Condition, "
      << "which doesn't match.";
  EXPECT_FALSE(result->IsFulfilled(-1, url_matches, 0))
      << "Testing the 'no ID' value tries to match the 4th condition, but "
      << "its max is too low.";
  EXPECT_TRUE(result->IsFulfilled(-1, url_matches, -5))
      << "Testing the 'no ID' value tries to match the 4th condition, and "
      << "this value is low enough.";

  url_matches.insert(1);
  EXPECT_TRUE(result->IsFulfilled(1, url_matches, 3))
      << "Tests a condition with a url matcher, for a matching value.";
  EXPECT_FALSE(result->IsFulfilled(1, url_matches, 4))
      << "Tests a condition with a url matcher, for a non-matching value "
      << "that would match a different condition.";
  url_matches.insert(2);
  EXPECT_TRUE(result->IsFulfilled(2, url_matches, 4))
      << "Tests with 2 elements in the match set.";

  // Check the condition sets:
  URLMatcherConditionSet::Vector condition_sets;
  result->GetURLMatcherConditionSets(&condition_sets);
  ASSERT_EQ(3U, condition_sets.size());
  EXPECT_EQ(1, condition_sets[0]->id());
  EXPECT_EQ(2, condition_sets[1]->id());
  EXPECT_EQ(3, condition_sets[2]->id());
}

// DeclarativeAction

struct SummingAction {
  typedef int ApplyInfo;

  int increment;
  int min_priority;

  static scoped_ptr<SummingAction> Create(const base::Value& action,
                                          std::string* error,
                                          bool* bad_message) {
    scoped_ptr<SummingAction> result(new SummingAction());
    const base::DictionaryValue* dict = NULL;
    EXPECT_TRUE(action.GetAsDictionary(&dict));
    if (dict->HasKey("error")) {
      EXPECT_TRUE(dict->GetString("error", error));
      return result.Pass();
    }
    if (dict->HasKey("bad")) {
      *bad_message = true;
      return result.Pass();
    }

    EXPECT_TRUE(dict->GetInteger("value", &result->increment));
    dict->GetInteger("priority", &result->min_priority);
    return result.Pass();
  }

  void Apply(const std::string& extension_id,
             const base::Time& install_time,
             int* sum) const {
    *sum += increment;
  }

  int GetMinimumPriority() const {
    return min_priority;
  }
};
typedef DeclarativeActionSet<SummingAction> SummingActionSet;

TEST(DeclarativeActionTest, ErrorActionSet) {
  SummingActionSet::AnyVector actions;
  actions.push_back(make_linked_ptr(new Any(ParseJson("{\"value\": 1}"))));
  actions.push_back(make_linked_ptr(new Any(ParseJson(
      "{\"error\": \"the error\"}"))));

  std::string error;
  bool bad = false;
  scoped_ptr<SummingActionSet> result =
      SummingActionSet::Create(actions, &error, &bad);
  EXPECT_EQ("the error", error);
  EXPECT_FALSE(bad);
  EXPECT_FALSE(result);

  actions.clear();
  actions.push_back(make_linked_ptr(new Any(ParseJson("{\"value\": 1}"))));
  actions.push_back(make_linked_ptr(new Any(ParseJson("{\"bad\": 3}"))));
  result = SummingActionSet::Create(actions, &error, &bad);
  EXPECT_EQ("", error);
  EXPECT_TRUE(bad);
  EXPECT_FALSE(result);
}

TEST(DeclarativeActionTest, ApplyActionSet) {
  SummingActionSet::AnyVector actions;
  actions.push_back(make_linked_ptr(new Any(ParseJson(
      "{\"value\": 1,"
      " \"priority\": 5}"))));
  actions.push_back(make_linked_ptr(new Any(ParseJson("{\"value\": 2}"))));

  // Test insertion
  std::string error;
  bool bad = false;
  scoped_ptr<SummingActionSet> result =
      SummingActionSet::Create(actions, &error, &bad);
  EXPECT_EQ("", error);
  EXPECT_FALSE(bad);
  ASSERT_TRUE(result);
  EXPECT_EQ(2u, result->actions().size());

  int sum = 0;
  result->Apply("ext_id", base::Time(), &sum);
  EXPECT_EQ(3, sum);
  EXPECT_EQ(5, result->GetMinimumPriority());
}

TEST(DeclarativeRuleTest, Create) {
  typedef DeclarativeRule<FulfillableCondition, SummingAction> Rule;
  linked_ptr<Rule::JsonRule> json_rule(new Rule::JsonRule);
  ASSERT_TRUE(Rule::JsonRule::Populate(
      *ParseJson("{ \n"
                 "  \"id\": \"rule1\", \n"
                 "  \"conditions\": [ \n"
                 "    {\"url_id\": 1, \"max\": 3}, \n"
                 "    {\"url_id\": 2, \"max\": 5}, \n"
                 "  ], \n"
                 "  \"actions\": [ \n"
                 "    { \n"
                 "      \"value\": 2 \n"
                 "    } \n"
                 "  ], \n"
                 "  \"priority\": 200 \n"
                 "}"),
      json_rule.get()));

  const char kExtensionId[] = "ext1";

  base::Time install_time = base::Time::Now();

  URLMatcher matcher;
  std::string error;
  scoped_ptr<Rule> rule(Rule::Create(matcher.condition_factory(), kExtensionId,
                                     install_time, json_rule, NULL, &error));
  EXPECT_EQ("", error);
  ASSERT_TRUE(rule.get());

  EXPECT_EQ(kExtensionId, rule->id().first);
  EXPECT_EQ("rule1", rule->id().second);

  EXPECT_EQ(200, rule->priority());

  const Rule::ConditionSet& condition_set = rule->conditions();
  const Rule::ConditionSet::Conditions& conditions =
      condition_set.conditions();
  ASSERT_EQ(2u, conditions.size());
  EXPECT_EQ(3, conditions[0]->max_value);
  EXPECT_EQ(5, conditions[1]->max_value);

  const Rule::ActionSet& action_set = rule->actions();
  const Rule::ActionSet::Actions& actions = action_set.actions();
  ASSERT_EQ(1u, actions.size());
  EXPECT_EQ(2, actions[0]->increment);

  int sum = 0;
  rule->Apply(&sum);
  EXPECT_EQ(2, sum);
}

bool AtLeastOneCondition(
    const DeclarativeConditionSet<FulfillableCondition>* conditions,
    const DeclarativeActionSet<SummingAction>* actions,
    std::string* error) {
  if (conditions->conditions().empty()) {
    *error = "No conditions";
    return false;
  }
  return true;
}

TEST(DeclarativeRuleTest, CheckConsistency) {
  typedef DeclarativeRule<FulfillableCondition, SummingAction> Rule;
  URLMatcher matcher;
  std::string error;
  linked_ptr<Rule::JsonRule> json_rule(new Rule::JsonRule);
  const char kExtensionId[] = "ext1";

  ASSERT_TRUE(Rule::JsonRule::Populate(
      *ParseJson("{ \n"
                 "  \"id\": \"rule1\", \n"
                 "  \"conditions\": [ \n"
                 "    {\"url_id\": 1, \"max\": 3}, \n"
                 "    {\"url_id\": 2, \"max\": 5}, \n"
                 "  ], \n"
                 "  \"actions\": [ \n"
                 "    { \n"
                 "      \"value\": 2 \n"
                 "    } \n"
                 "  ], \n"
                 "  \"priority\": 200 \n"
                 "}"),
      json_rule.get()));
  scoped_ptr<Rule> rule(
      Rule::Create(matcher.condition_factory(), kExtensionId, base::Time(),
                   json_rule, &AtLeastOneCondition, &error));
  EXPECT_TRUE(rule);
  EXPECT_EQ("", error);

  ASSERT_TRUE(Rule::JsonRule::Populate(
      *ParseJson("{ \n"
                 "  \"id\": \"rule1\", \n"
                 "  \"conditions\": [ \n"
                 "  ], \n"
                 "  \"actions\": [ \n"
                 "    { \n"
                 "      \"value\": 2 \n"
                 "    } \n"
                 "  ], \n"
                 "  \"priority\": 200 \n"
                 "}"),
      json_rule.get()));
  rule = Rule::Create(matcher.condition_factory(), kExtensionId, base::Time(),
                      json_rule, &AtLeastOneCondition, &error);
  EXPECT_FALSE(rule);
  EXPECT_EQ("No conditions", error);
}

}  // namespace extensions
