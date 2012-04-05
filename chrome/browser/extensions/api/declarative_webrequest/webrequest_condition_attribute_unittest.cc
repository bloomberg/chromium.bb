// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_condition_attribute.h"

#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_constants.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kUnknownConditionName[] = "unknownType";
}  // namespace

namespace extensions {

namespace keys = declarative_webrequest_constants;

TEST(WebRequestConditionAttributeTest, CreateConditionAttribute) {
  // Necessary for TestURLRequest.
  MessageLoop message_loop(MessageLoop::TYPE_IO);

  std::string error;
  scoped_ptr<WebRequestConditionAttribute> result;
  StringValue http_string_value("http");
  ListValue list_value;

  // Test wrong condition name passed.
  error.clear();
  result = WebRequestConditionAttribute::Create(
      kUnknownConditionName, &http_string_value, &error);
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(result.get());

  // Test wrong data type passed
  error.clear();
  result = WebRequestConditionAttribute::Create(
      kUnknownConditionName, &list_value, &error);
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(result.get());

  // Test success
  error.clear();
  result = WebRequestConditionAttribute::Create(
      keys::kSchemeKey, &http_string_value, &error);
  EXPECT_TRUE(error.empty());
  ASSERT_TRUE(result.get());
  EXPECT_EQ(WebRequestConditionAttribute::CONDITION_HAS_SCHEME,
            result->GetType());
  TestURLRequest url_request_ok(GURL("http://www.example.com"), NULL);
  EXPECT_TRUE(result->IsFulfilled(&url_request_ok, ON_BEFORE_REQUEST));
  TestURLRequest url_request_fail(GURL("https://www.example.com"), NULL);
  EXPECT_FALSE(result->IsFulfilled(&url_request_fail, ON_BEFORE_REQUEST));
}

}  // namespace extensions
