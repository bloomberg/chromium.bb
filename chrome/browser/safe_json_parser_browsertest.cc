// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "services/data_decoder/public/cpp/safe_json_parser.h"

namespace {

using data_decoder::SafeJsonParser;

std::string MaybeToJson(const base::Value* value) {
  if (!value)
    return "(null)";

  std::string json;
  if (!base::JSONWriter::Write(*value, &json))
    return "(invalid value)";

  return json;
}

}  // namespace

class SafeJsonParserTest : public InProcessBrowserTest {
 protected:
  void TestParse(const std::string& json) {
    SCOPED_TRACE(json);
    DCHECK(!message_loop_runner_);
    message_loop_runner_ = new content::MessageLoopRunner;

    std::string error;
    std::unique_ptr<base::Value> value = base::JSONReader::ReadAndReturnError(
        json, base::JSON_PARSE_RFC, nullptr, &error);

    SafeJsonParser::SuccessCallback success_callback;
    SafeJsonParser::ErrorCallback error_callback;
    if (value) {
      success_callback =
          base::Bind(&SafeJsonParserTest::ExpectValue, base::Unretained(this),
                     base::Passed(&value));
      error_callback = base::Bind(&SafeJsonParserTest::FailWithError,
                                  base::Unretained(this));
    } else {
      success_callback = base::Bind(&SafeJsonParserTest::FailWithValue,
                                    base::Unretained(this));
      error_callback = base::Bind(&SafeJsonParserTest::ExpectError,
                                  base::Unretained(this), error);
    }
    SafeJsonParser::Parse(
        content::ServiceManagerConnection::GetForProcess()->GetConnector(),
        json, success_callback, error_callback);

    message_loop_runner_->Run();
    message_loop_runner_ = nullptr;
  }

 private:
  void ExpectValue(std::unique_ptr<base::Value> expected_value,
                   std::unique_ptr<base::Value> actual_value) {
    EXPECT_EQ(*expected_value, *actual_value)
        << "Expected: " << MaybeToJson(expected_value.get())
        << " Actual: " << MaybeToJson(actual_value.get());
    message_loop_runner_->Quit();
  }

  void ExpectError(const std::string& expected_error,
                   const std::string& actual_error) {
    EXPECT_EQ(expected_error, actual_error);
    message_loop_runner_->Quit();
  }

  void FailWithValue(std::unique_ptr<base::Value> value) {
    ADD_FAILURE() << MaybeToJson(value.get());
    message_loop_runner_->Quit();
  }

  void FailWithError(const std::string& error) {
    ADD_FAILURE() << error;
    message_loop_runner_->Quit();
  }

  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
};

IN_PROC_BROWSER_TEST_F(SafeJsonParserTest, Parse) {
  TestParse("{}");
  TestParse("choke");
  TestParse("{\"awesome\": true}");
  TestParse("\"laser\"");
  TestParse("false");
  TestParse("null");
  TestParse("3.14");
  TestParse("[");
  TestParse("\"");
  TestParse(std::string());
  TestParse("☃");
  TestParse("\"☃\"");
  TestParse("\"\\ufdd0\"");
  TestParse("\"\\ufffe\"");
  TestParse("\"\\ud83f\\udffe\"");
}
