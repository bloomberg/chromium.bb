// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/save_password_progress_logger.h"

#include <limits>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::UTF8ToUTF16;

namespace autofill {

namespace {

const char kTestString[] = "Test";

class TestLogger : public SavePasswordProgressLogger {
 public:
  bool LogsContainSubstring(const std::string& substring) {
    return accumulated_log_.find(substring) != std::string::npos;
  }

  std::string accumulated_log() { return accumulated_log_; }

 private:
  virtual void SendLog(const std::string& log) OVERRIDE {
    accumulated_log_.append(log);
  }

  std::string accumulated_log_;
};

};  // namespace

TEST(SavePasswordProgressLoggerTest, LogPasswordForm) {
  TestLogger logger;
  PasswordForm form;
  form.action = GURL("http://example.org/verysecret?verysecret");
  form.password_value = UTF8ToUTF16("verysecret");
  form.username_value = UTF8ToUTF16("verysecret");
  logger.LogPasswordForm(kTestString, form);
  SCOPED_TRACE(testing::Message() << "Log string = ["
                                  << logger.accumulated_log() << "]");
  EXPECT_TRUE(logger.LogsContainSubstring(kTestString));
  EXPECT_TRUE(logger.LogsContainSubstring("http://example.org"));
  EXPECT_FALSE(logger.LogsContainSubstring("verysecret"));
}

TEST(SavePasswordProgressLoggerTest, LogHTMLForm) {
  TestLogger logger;
  logger.LogHTMLForm(kTestString,
                     "form_name",
                     "form_method",
                     GURL("http://example.org/verysecret?verysecret"));
  SCOPED_TRACE(testing::Message() << "Log string = ["
                                  << logger.accumulated_log() << "]");
  EXPECT_TRUE(logger.LogsContainSubstring(kTestString));
  EXPECT_TRUE(logger.LogsContainSubstring("form_name"));
  EXPECT_TRUE(logger.LogsContainSubstring("form_method"));
  EXPECT_TRUE(logger.LogsContainSubstring("http://example.org"));
  EXPECT_FALSE(logger.LogsContainSubstring("verysecret"));
}

TEST(SavePasswordProgressLoggerTest, LogURL) {
  TestLogger logger;
  logger.LogURL(kTestString, GURL("http://example.org/verysecret?verysecret"));
  SCOPED_TRACE(testing::Message() << "Log string = ["
                                  << logger.accumulated_log() << "]");
  EXPECT_TRUE(logger.LogsContainSubstring(kTestString));
  EXPECT_TRUE(logger.LogsContainSubstring("http://example.org"));
  EXPECT_FALSE(logger.LogsContainSubstring("verysecret"));
}

TEST(SavePasswordProgressLoggerTest, LogBooleanTrue) {
  TestLogger logger;
  logger.LogBoolean(kTestString, true);
  SCOPED_TRACE(testing::Message() << "Log string = ["
                                  << logger.accumulated_log() << "]");
  EXPECT_TRUE(logger.LogsContainSubstring(kTestString));
  EXPECT_TRUE(logger.LogsContainSubstring("true"));
}

TEST(SavePasswordProgressLoggerTest, LogBooleanFalse) {
  TestLogger logger;
  logger.LogBoolean(kTestString, false);
  SCOPED_TRACE(testing::Message() << "Log string = ["
                                  << logger.accumulated_log() << "]");
  EXPECT_TRUE(logger.LogsContainSubstring(kTestString));
  EXPECT_TRUE(logger.LogsContainSubstring("false"));
}

TEST(SavePasswordProgressLoggerTest, LogSignedNumber) {
  TestLogger logger;
  int signed_number = -12345;
  logger.LogNumber(kTestString, signed_number);
  SCOPED_TRACE(testing::Message() << "Log string = ["
                                  << logger.accumulated_log() << "]");
  EXPECT_TRUE(logger.LogsContainSubstring(kTestString));
  EXPECT_TRUE(logger.LogsContainSubstring("-12345"));
}

TEST(SavePasswordProgressLoggerTest, LogUnsignedNumber) {
  TestLogger logger;
  size_t unsigned_number = 654321;
  logger.LogNumber(kTestString, unsigned_number);
  SCOPED_TRACE(testing::Message() << "Log string = ["
                                  << logger.accumulated_log() << "]");
  EXPECT_TRUE(logger.LogsContainSubstring(kTestString));
  EXPECT_TRUE(logger.LogsContainSubstring("654321"));
}

TEST(SavePasswordProgressLoggerTest, LogFinalDecisionSave) {
  TestLogger logger;
  logger.LogFinalDecision(SavePasswordProgressLogger::DECISION_SAVE);
  SCOPED_TRACE(testing::Message() << "Log string = ["
                                  << logger.accumulated_log() << "]");
  EXPECT_TRUE(logger.LogsContainSubstring("SAVE"));
}

TEST(SavePasswordProgressLoggerTest, LogFinalDecisionAsk) {
  TestLogger logger;
  logger.LogFinalDecision(SavePasswordProgressLogger::DECISION_ASK);
  SCOPED_TRACE(testing::Message() << "Log string = ["
                                  << logger.accumulated_log() << "]");
  EXPECT_TRUE(logger.LogsContainSubstring("ASK"));
}

TEST(SavePasswordProgressLoggerTest, LogFinalDecisionDrop) {
  TestLogger logger;
  logger.LogFinalDecision(SavePasswordProgressLogger::DECISION_DROP);
  SCOPED_TRACE(testing::Message() << "Log string = ["
                                  << logger.accumulated_log() << "]");
  EXPECT_TRUE(logger.LogsContainSubstring("DROP"));
}

TEST(SavePasswordProgressLoggerTest, LogMessage) {
  TestLogger logger;
  logger.LogMessage(kTestString);
  SCOPED_TRACE(testing::Message() << "Log string = ["
                                  << logger.accumulated_log() << "]");
  EXPECT_TRUE(logger.LogsContainSubstring(kTestString));
}

}  // namespace autofill
