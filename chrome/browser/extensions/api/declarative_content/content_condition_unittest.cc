// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/content_condition.h"

#include "base/bind.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class TestPredicate : public ContentPredicate {
 public:
  TestPredicate() {}

  ContentPredicateEvaluator* GetEvaluator() const override {
    return nullptr;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestPredicate);
};

class TestPredicateFactoryGeneratingError : public ContentPredicateFactory {
 public:
  explicit TestPredicateFactoryGeneratingError(const std::string& error)
      : error_(error) {
  }

  scoped_ptr<const ContentPredicate> CreatePredicate(
      const Extension* extension,
      const base::Value& value,
      std::string* error) override {
    *error = error_;
    return scoped_ptr<const ContentPredicate>();
  }

 private:
  const std::string error_;

  DISALLOW_COPY_AND_ASSIGN(TestPredicateFactoryGeneratingError);
};

class TestPredicateFactoryGeneratingPredicate : public ContentPredicateFactory {
 public:
  TestPredicateFactoryGeneratingPredicate() {}

  scoped_ptr<const ContentPredicate> CreatePredicate(
      const Extension* extension,
      const base::Value& value,
      std::string* error) override {
    scoped_ptr<const ContentPredicate> predicate(new TestPredicate);
    created_predicates_.push_back(predicate.get());
    return predicate;
  }

  const std::vector<const ContentPredicate*>& created_predicates() const {
    return created_predicates_;
  }

 private:
  std::vector<const ContentPredicate*> created_predicates_;

  DISALLOW_COPY_AND_ASSIGN(TestPredicateFactoryGeneratingPredicate);
};

}  // namespace

using testing::HasSubstr;
using testing::UnorderedElementsAre;

TEST(DeclarativeContentConditionTest, UnknownPredicateName) {
  std::string error;
  scoped_ptr<ContentCondition> condition = CreateContentCondition(
      nullptr,
      std::map<std::string, ContentPredicateFactory*>(),
      *base::test::ParseJson(
           "{\n"
           "  \"invalid\": \"foobar\",\n"
           "  \"instanceType\": \"declarativeContent.PageStateMatcher\",\n"
           "}"),
      &error);
  EXPECT_THAT(error, HasSubstr("Unknown condition attribute"));
  EXPECT_FALSE(condition);
}

TEST(DeclarativeContentConditionTest,
     PredicateWithErrorProducesEmptyCondition) {
  TestPredicateFactoryGeneratingError factory("error message");
  std::map<std::string, ContentPredicateFactory*> predicate_factories;
  predicate_factories["test_predicate"] = &factory;
  std::string error;
  scoped_ptr<ContentCondition> condition = CreateContentCondition(
      nullptr,
      predicate_factories,
      *base::test::ParseJson(
           "{\n"
           "  \"test_predicate\": \"\",\n"
           "  \"instanceType\": \"declarativeContent.PageStateMatcher\",\n"
           "}"),
      &error);
  EXPECT_EQ("error message", error);
  EXPECT_FALSE(condition);
}

TEST(DeclarativeContentConditionTest, AllSpecifiedPredicatesCreated) {
  TestPredicateFactoryGeneratingPredicate factory1, factory2;
  std::map<std::string, ContentPredicateFactory*> predicate_factories;
  predicate_factories["test_predicate1"] = &factory1;
  predicate_factories["test_predicate2"] = &factory2;
  std::string error;
  scoped_ptr<ContentCondition> condition = CreateContentCondition(
      nullptr,
      predicate_factories,
      *base::test::ParseJson(
           "{\n"
           "  \"test_predicate1\": {},\n"
           "  \"test_predicate2\": [],\n"
           "  \"instanceType\": \"declarativeContent.PageStateMatcher\",\n"
           "}"),
      &error);
  ASSERT_TRUE(condition);
  ASSERT_EQ(1u, factory1.created_predicates().size());
  ASSERT_EQ(1u, factory2.created_predicates().size());
  EXPECT_THAT(condition->predicates,
              UnorderedElementsAre(factory1.created_predicates()[0],
                                   factory2.created_predicates()[0]));
}

}  // namespace extensions
