// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/format_macros.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "base/json/json_reader.h"
#include "chrome/test/webdriver/dispatch.h"
#include "chrome/test/webdriver/http_response.h"
#include "chrome/test/webdriver/commands/response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webdriver {

namespace {

void ExpectHeaderValue(const HttpResponse& response, const std::string& name,
                       const std::string& expected_value) {
  std::string actual_value;
  EXPECT_TRUE(response.GetHeader(name, &actual_value));
  EXPECT_EQ(expected_value, actual_value);
}

void ExpectHttpStatus(int expected_status,
                      const Response& command_response,
                      HttpResponse* const http_response) {
  internal::PrepareHttpResponse(command_response, http_response);
  EXPECT_EQ(expected_status, http_response->status());
}

void ExpectInternalError(ErrorCode command_status,
                         Response* command_response,
                         HttpResponse* const http_response) {
  command_response->SetStatus(command_status);
  http_response->set_status(HttpResponse::kOk);  // Reset to detect changes.
  ExpectHttpStatus(HttpResponse::kInternalServerError,
                   *command_response, http_response);
}

}  // namespace

TEST(DispatchTest, CorrectlyConvertsResponseCodesToHttpStatusCodes) {
  HttpResponse http_response;

  Response command_response;
  command_response.SetValue(Value::CreateStringValue("foobar"));

  command_response.SetStatus(kSuccess);
  ExpectHttpStatus(HttpResponse::kOk, command_response, &http_response);

  command_response.SetStatus(kSeeOther);
  ExpectHttpStatus(HttpResponse::kSeeOther, command_response, &http_response);
  ExpectHeaderValue(http_response, "location", "foobar");
  http_response.ClearHeaders();

  command_response.SetStatus(kBadRequest);
  ExpectHttpStatus(HttpResponse::kBadRequest, command_response,
                   &http_response);

  command_response.SetStatus(kSessionNotFound);
  ExpectHttpStatus(HttpResponse::kNotFound, command_response,
                   &http_response);

  ListValue* methods = new ListValue;
  methods->Append(Value::CreateStringValue("POST"));
  methods->Append(Value::CreateStringValue("GET"));
  command_response.SetValue(methods);
  command_response.SetStatus(kMethodNotAllowed);
  ExpectHttpStatus(HttpResponse::kMethodNotAllowed, command_response,
                   &http_response);
  ExpectHeaderValue(http_response, "allow", "POST,GET");
  http_response.ClearHeaders();

  ExpectInternalError(kNoSuchElement, &command_response, &http_response);
  ExpectInternalError(kNoSuchFrame, &command_response, &http_response);
  ExpectInternalError(kUnknownCommand, &command_response, &http_response);
  ExpectInternalError(kStaleElementReference, &command_response,
                      &http_response);
  ExpectInternalError(kInvalidElementState, &command_response, &http_response);
  ExpectInternalError(kUnknownError, &command_response, &http_response);
  ExpectInternalError(kElementNotSelectable, &command_response,
                      &http_response);
  ExpectInternalError(kXPathLookupError, &command_response, &http_response);
  ExpectInternalError(kNoSuchWindow, &command_response, &http_response);
  ExpectInternalError(kInvalidCookieDomain, &command_response, &http_response);
  ExpectInternalError(kUnableToSetCookie, &command_response, &http_response);
  ExpectInternalError(kInternalServerError, &command_response, &http_response);
}

TEST(DispatchTest,
     ReturnsAnErrorOnNonStringMethodsListedOnAMethodNotAllowedResponse) {
  ListValue* methods = new ListValue;
  methods->Append(Value::CreateStringValue("POST"));
  methods->Append(new DictionaryValue);
  methods->Append(Value::CreateStringValue("GET"));
  methods->Append(new DictionaryValue);
  methods->Append(Value::CreateStringValue("DELETE"));

  Response command_response;
  command_response.SetStatus(kMethodNotAllowed);
  command_response.SetValue(methods);

  HttpResponse http_response;
  ExpectHttpStatus(HttpResponse::kInternalServerError, command_response,
                   &http_response);
}

TEST(DispatchTest, ReturnsCommandResponseAsJson) {
  const std::string kExpectedData = "{\"status\":0,\"value\":\"foobar\"}";

  Response command_response;
  command_response.SetStatus(kSuccess);
  command_response.SetValue(Value::CreateStringValue("foobar"));

  HttpResponse http_response;
  internal::PrepareHttpResponse(command_response, &http_response);
  EXPECT_EQ(HttpResponse::kOk, http_response.status());
  ExpectHeaderValue(http_response, "content-type",
                    "application/json; charset=utf-8");
  ExpectHeaderValue(http_response, "content-length",
                    base::StringPrintf("%"PRIuS, kExpectedData.length()));

  // We do not know whether the response status or value will be
  // encoded first, so we have to parse the response body to
  // verify it is correct.
  std::string actual_data(http_response.data(),
                          http_response.length());

  int error_code;
  std::string error_message;
  scoped_ptr<Value> parsed_response(base::JSONReader::ReadAndReturnError(
      actual_data, false, &error_code, &error_message));

  ASSERT_TRUE(parsed_response.get() != NULL) << error_message;
  ASSERT_TRUE(parsed_response->IsType(Value::TYPE_DICTIONARY))
      << "Response should be a dictionary: " << actual_data;

  DictionaryValue* dict = static_cast<DictionaryValue*>(parsed_response.get());
  EXPECT_EQ(2u, dict->size());
  EXPECT_TRUE(dict->HasKey("status"));
  EXPECT_TRUE(dict->HasKey("value"));

  int status = -1;
  EXPECT_TRUE(dict->GetInteger("status", &status));
  EXPECT_EQ(kSuccess, static_cast<ErrorCode>(status));

  std::string value;
  EXPECT_TRUE(dict->GetStringASCII("value", &value));
  EXPECT_EQ("foobar", value);
}

}  // namespace webdriver
