// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_condition_attribute.h"

#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_constants.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_rule.h"
#include "content/public/browser/resource_request_info.h"
#include "net/url_request/url_request_test_util.h"
#include "net/test/test_server.h"
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

  error.clear();
  result = WebRequestConditionAttribute::Create(
      keys::kContentTypeKey, &string_value, &error);
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

TEST(WebRequestConditionAttributeTest, ResourceType) {
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

  TestURLRequestContext context;
  TestURLRequest url_request_ok(GURL("http://www.example.com"), NULL, &context);
  content::ResourceRequestInfo::AllocateForTesting(&url_request_ok,
      ResourceType::MAIN_FRAME, NULL, -1, -1);
  EXPECT_TRUE(attribute->IsFulfilled(
      WebRequestRule::RequestData(&url_request_ok, ON_BEFORE_REQUEST)));

  TestURLRequest url_request_fail(
      GURL("http://www.example.com"), NULL, &context);
  content::ResourceRequestInfo::AllocateForTesting(&url_request_ok,
      ResourceType::SUB_FRAME, NULL, -1, -1);
  EXPECT_FALSE(attribute->IsFulfilled(
      WebRequestRule::RequestData(&url_request_fail, ON_BEFORE_REQUEST)));
}

TEST(WebRequestConditionAttributeTest, ContentType) {
  // Necessary for TestURLRequest.
  MessageLoop message_loop(MessageLoop::TYPE_IO);

  std::string error;
  scoped_ptr<WebRequestConditionAttribute> result;

  net::TestServer test_server(
      net::TestServer::TYPE_HTTP,
      net::TestServer::kLocalhost,
      FilePath(FILE_PATH_LITERAL(
          "chrome/test/data/extensions/api_test/webrequest/declarative")));
  ASSERT_TRUE(test_server.Start());

  TestURLRequestContext context;
  TestDelegate delegate;
  TestURLRequest url_request(test_server.GetURL("files/headers.html"),
                                                &delegate, &context);
  url_request.Start();
  MessageLoop::current()->Run();

  ListValue content_types;
  content_types.Append(Value::CreateStringValue("text/plain"));
  scoped_ptr<WebRequestConditionAttribute> attribute_include =
      WebRequestConditionAttribute::Create(
          keys::kContentTypeKey, &content_types, &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(attribute_include.get());
  EXPECT_FALSE(attribute_include->IsFulfilled(
      WebRequestRule::RequestData(&url_request, ON_BEFORE_REQUEST,
                                  url_request.response_headers())));
  EXPECT_TRUE(attribute_include->IsFulfilled(
      WebRequestRule::RequestData(&url_request, ON_HEADERS_RECEIVED,
                                  url_request.response_headers())));

  scoped_ptr<WebRequestConditionAttribute> attribute_exclude =
      WebRequestConditionAttribute::Create(
          keys::kExcludeContentTypeKey, &content_types, &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(attribute_exclude.get());
  EXPECT_FALSE(attribute_exclude->IsFulfilled(
      WebRequestRule::RequestData(&url_request, ON_HEADERS_RECEIVED,
                                  url_request.response_headers())));

  content_types.Clear();
  content_types.Append(Value::CreateStringValue("something/invalid"));
  scoped_ptr<WebRequestConditionAttribute> attribute_unincluded =
      WebRequestConditionAttribute::Create(
          keys::kContentTypeKey, &content_types, &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(attribute_unincluded.get());
  EXPECT_FALSE(attribute_unincluded->IsFulfilled(
      WebRequestRule::RequestData(&url_request, ON_HEADERS_RECEIVED,
                                  url_request.response_headers())));

  scoped_ptr<WebRequestConditionAttribute> attribute_unexcluded =
      WebRequestConditionAttribute::Create(
          keys::kExcludeContentTypeKey, &content_types, &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(attribute_unexcluded.get());
  EXPECT_TRUE(attribute_unexcluded->IsFulfilled(
      WebRequestRule::RequestData(&url_request, ON_HEADERS_RECEIVED,
                                  url_request.response_headers())));
}

}  // namespace extensions
