// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_action.h"

#include "base/values.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kUnknownActionType[] = "unknownType";
}  // namespace

namespace extensions {

namespace keys = declarative_webrequest_constants;

TEST(WebRequestActionTest, CreateAction) {
  std::string error;
  bool bad_message = false;
  scoped_ptr<WebRequestAction> result;

  // Test wrong data type passed.
  error.clear();
  base::ListValue empty_list;
  result = WebRequestAction::Create(empty_list, &error, &bad_message);
  EXPECT_TRUE(bad_message);
  EXPECT_FALSE(result.get());

  // Test missing instanceType element.
  base::DictionaryValue input;
  error.clear();
  result = WebRequestAction::Create(input, &error, &bad_message);
  EXPECT_TRUE(bad_message);
  EXPECT_FALSE(result.get());

  // Test wrong instanceType element.
  input.SetString(keys::kInstanceTypeKey, kUnknownActionType);
  error.clear();
  result = WebRequestAction::Create(input, &error, &bad_message);
  EXPECT_NE("", error);
  EXPECT_FALSE(result.get());

  // Test success
  input.SetString(keys::kInstanceTypeKey, keys::kCancelRequestType);
  error.clear();
  result = WebRequestAction::Create(input, &error, &bad_message);
  EXPECT_EQ("", error);
  EXPECT_FALSE(bad_message);
  ASSERT_TRUE(result.get());
  EXPECT_EQ(WebRequestAction::ACTION_CANCEL_REQUEST, result->GetType());
}

TEST(WebRequestActionTest, CreateActionSet) {
  std::string error;
  bool bad_message = false;
  scoped_ptr<WebRequestActionSet> result;

  WebRequestActionSet::AnyVector input;

  // Test empty input.
  error.clear();
  result = WebRequestActionSet::Create(input, &error, &bad_message);
  EXPECT_TRUE(error.empty()) << error;
  EXPECT_FALSE(bad_message);
  ASSERT_TRUE(result.get());
  EXPECT_TRUE(result->actions().empty());
  EXPECT_EQ(std::numeric_limits<int>::min(), result->GetMinimumPriority());

  DictionaryValue correct_action;
  correct_action.SetString(keys::kInstanceTypeKey, keys::kIgnoreRulesType);
  correct_action.SetInteger(keys::kLowerPriorityThanKey, 10);
  DictionaryValue incorrect_action;
  incorrect_action.SetString(keys::kInstanceTypeKey, kUnknownActionType);

  // Test success.
  linked_ptr<json_schema_compiler::any::Any> action1 = make_linked_ptr(
      new json_schema_compiler::any::Any);
  action1->Init(correct_action);
  input.push_back(action1);
  error.clear();
  result = WebRequestActionSet::Create(input, &error, &bad_message);
  EXPECT_TRUE(error.empty()) << error;
  EXPECT_FALSE(bad_message);
  ASSERT_TRUE(result.get());
  ASSERT_EQ(1u, result->actions().size());
  EXPECT_EQ(WebRequestAction::ACTION_IGNORE_RULES,
            result->actions()[0]->GetType());
  EXPECT_EQ(10, result->GetMinimumPriority());

  // Test failure.
  linked_ptr<json_schema_compiler::any::Any> action2 = make_linked_ptr(
      new json_schema_compiler::any::Any);
  action2->Init(incorrect_action);
  input.push_back(action2);
  error.clear();
  result = WebRequestActionSet::Create(input, &error, &bad_message);
  EXPECT_NE("", error);
  EXPECT_FALSE(result.get());
}

// Test capture group syntax conversions of WebRequestRedirectByRegExAction
TEST(WebRequestActionTest, PerlToRe2Style) {
#define CallPerlToRe2Style WebRequestRedirectByRegExAction::PerlToRe2Style
  // foo$1bar -> foo\1bar
  EXPECT_EQ("foo\\1bar", CallPerlToRe2Style("foo$1bar"));
  // foo\$1bar -> foo$1bar
  EXPECT_EQ("foo$1bar", CallPerlToRe2Style("foo\\$1bar"));
  // foo\\$1bar -> foo\\\1bar
  EXPECT_EQ("foo\\\\\\1bar", CallPerlToRe2Style("foo\\\\$1bar"));
  // foo\bar -> foobar
  EXPECT_EQ("foobar", CallPerlToRe2Style("foo\\bar"));
  // foo$bar -> foo$bar
  EXPECT_EQ("foo$bar", CallPerlToRe2Style("foo$bar"));
#undef CallPerlToRe2Style
}

}  // namespace extensions
