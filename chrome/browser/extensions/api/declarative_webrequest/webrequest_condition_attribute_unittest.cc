// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_condition_attribute.h"

#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_constants.h"
#include "content/public/browser/resource_request_info.h"
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
  StringValue string_value("main_frame");
  ListValue resource_types;
  resource_types.Append(Value::CreateStringValue("main_frame"));

  // Test wrong condition name passed.
  error.clear();
  result = WebRequestConditionAttribute::Create(
      kUnknownConditionName, &resource_types, &error);
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(result.get());

  // Test wrong data type passed
  error.clear();
  result = WebRequestConditionAttribute::Create(
      keys::kResourceTypeKey, &string_value, &error);
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(result.get());

  // Test success
  error.clear();
  result = WebRequestConditionAttribute::Create(
      keys::kResourceTypeKey, &resource_types, &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(result.get());
  EXPECT_EQ(WebRequestConditionAttribute::CONDITION_RESOURCE_TYPE,
            result->GetType());
}

TEST(WebRequestConditionAttributeTest, TestResourceType) {
  // Necessary for TestURLRequest.
  MessageLoop message_loop(MessageLoop::TYPE_IO);

  std::string error;
  ListValue resource_types;
  resource_types.Append(Value::CreateStringValue("main_frame"));

  scoped_ptr<WebRequestConditionAttribute> attribute =
      WebRequestConditionAttribute::Create(
          keys::kResourceTypeKey, &resource_types, &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(attribute.get());

  TestURLRequest url_request_ok(GURL("http://www.example.com"), NULL);
  content::ResourceRequestInfo::AllocateForTesting(&url_request_ok,
      ResourceType::MAIN_FRAME, NULL, -1, -1);
  EXPECT_TRUE(attribute->IsFulfilled(&url_request_ok, ON_BEFORE_REQUEST));

  TestURLRequest url_request_fail(GURL("http://www.example.com"), NULL);
  content::ResourceRequestInfo::AllocateForTesting(&url_request_ok,
      ResourceType::SUB_FRAME, NULL, -1, -1);
  EXPECT_FALSE(attribute->IsFulfilled(&url_request_fail, ON_BEFORE_REQUEST));
}

}  // namespace extensions
