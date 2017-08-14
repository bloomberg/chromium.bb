// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/contextual/contextual_json_request.h"

#include <utility>

#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/values.h"
#include "components/ntp_snippets/features.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_snippets {

namespace internal {

namespace {

using testing::_;
using testing::Eq;
using testing::StrEq;

MATCHER_P(EqualsJSON, json, "equals JSON") {
  std::unique_ptr<base::Value> expected = base::JSONReader::Read(json);
  if (!expected) {
    *result_listener << "INTERNAL ERROR: couldn't parse expected JSON";
    return false;
  }

  std::string err_msg;
  int err_line, err_col;
  std::unique_ptr<base::Value> actual = base::JSONReader::ReadAndReturnError(
      arg, base::JSON_PARSE_RFC, nullptr, &err_msg, &err_line, &err_col);
  if (!actual) {
    *result_listener << "input:" << err_line << ":" << err_col << ": "
                     << "parse error: " << err_msg;
    return false;
  }
  return *expected == *actual;
}

}  // namespace

class ContextualJsonRequestTest : public testing::Test {
 public:
  ContextualJsonRequestTest()
      : request_context_getter_(
            new net::TestURLRequestContextGetter(loop_.task_runner())) {}

  ContextualJsonRequest::Builder CreateDefaultBuilder() {
    ContextualJsonRequest::Builder builder;
    builder.SetUrl(GURL("http://valid-url.test"))
        .SetUrlRequestContextGetter(request_context_getter_.get());
    return builder;
  }

 private:
  base::MessageLoop loop_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(ContextualJsonRequestTest);
};

TEST_F(ContextualJsonRequestTest, AuthenticatedRequest) {
  ContextualJsonRequest::Builder builder = CreateDefaultBuilder();
  builder.SetAuthentication("0BFUSGAIA", "headerstuff")
      .SetContentUrl(GURL("http://my-url.test"))
      .Build();

  EXPECT_THAT(builder.PreviewRequestHeadersForTesting(),
              StrEq("Content-Type: application/json; charset=UTF-8\r\n"
                    "Authorization: headerstuff\r\n"
                    "\r\n"));
  EXPECT_THAT(builder.PreviewRequestBodyForTesting(),
              EqualsJSON("{"
                         "   \"categories\": [ \"RELATED_ARTICLES\","
                         " \"PUBLIC_DEBATE\" ],"
                         "   \"url\": \"http://my-url.test/\""
                         "}"));
}

}  // namespace internal

}  // namespace ntp_snippets
