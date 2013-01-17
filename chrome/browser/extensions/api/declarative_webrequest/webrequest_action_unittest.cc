// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_action.h"

#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_condition.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_constants.h"
#include "chrome/browser/extensions/api/web_request/web_request_api_helpers.h"
#include "chrome/common/extensions/extension_constants.h"
#include "net/url_request/url_request_test_util.h"
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
  input.push_back(linked_ptr<base::Value>(correct_action.DeepCopy()));
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
  input.push_back(linked_ptr<base::Value>(incorrect_action.DeepCopy()));
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

TEST(WebRequestActionTest, TestPermissions) {
  // Necessary for TestURLRequest.
  MessageLoop message_loop(MessageLoop::TYPE_IO);
  net::TestURLRequestContext context;

  std::string error;
  bool bad_message = false;
  scoped_ptr<WebRequestActionSet> action_set;

  // Setup redirect to http://www.foobar.com.
  base::DictionaryValue redirect;
  redirect.SetString(keys::kInstanceTypeKey, keys::kRedirectRequestType);
  redirect.SetString(keys::kRedirectUrlKey, "http://www.foobar.com");

  WebRequestActionSet::AnyVector actions;
  actions.push_back(linked_ptr<base::Value>(redirect.DeepCopy()));

  action_set = WebRequestActionSet::Create(actions, &error, &bad_message);
  EXPECT_EQ("", error);
  EXPECT_FALSE(bad_message);

  // Check that redirect works on regular URLs but not on protected URLs.
  net::TestURLRequest regular_request(GURL("http://test.com"), NULL, &context);
  std::list<LinkedPtrEventResponseDelta> deltas;
  DeclarativeWebRequestData request_data(&regular_request, ON_BEFORE_REQUEST);
  WebRequestAction::ApplyInfo apply_info = {
    NULL, request_data, false, &deltas
  };
  action_set->Apply("ext1", base::Time(), &apply_info);
  EXPECT_EQ(1u, deltas.size());

  net::TestURLRequest protected_request(GURL("http://clients1.google.com"),
                                        NULL, &context);
  deltas.clear();
  request_data =
      DeclarativeWebRequestData(&protected_request, ON_BEFORE_REQUEST);
  action_set->Apply("ext1", base::Time(), &apply_info);
  EXPECT_EQ(0u, deltas.size());
}

}  // namespace extensions
