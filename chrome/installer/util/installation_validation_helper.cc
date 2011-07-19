// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file declares helper functions for use in tests that expect a valid
// installation, possibly of a specific type.  Validation violations result in
// test failures.

#include "chrome/installer/util/installation_validation_helper.h"

#include "base/logging.h"
#include "base/string_piece.h"
#include "chrome/installer/util/installation_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer {

namespace {

// A helper class that installs a log message handler to add a test failure for
// each ERROR message.  Only one instance of this class may be live at a time.
class FailureLogHelper {
 public:
  FailureLogHelper();
  ~FailureLogHelper();

 private:
  static bool AddFailureForLogMessage(int severity,
                                      const char* file,
                                      int line,
                                      size_t message_start,
                                      const std::string& str);

  static const logging::LogSeverity kViolationSeverity_;
  static logging::LogMessageHandlerFunction old_message_handler_;
  static int old_min_log_level_;
};

// InstallationValidator logs all violations at ERROR level.
// static
const logging::LogSeverity
    FailureLogHelper::kViolationSeverity_ = logging::LOG_ERROR;

// static
logging::LogMessageHandlerFunction
    FailureLogHelper::old_message_handler_ = NULL;

// static
int FailureLogHelper::old_min_log_level_ =
    FailureLogHelper::kViolationSeverity_;

FailureLogHelper::FailureLogHelper() {
  LOG_ASSERT(old_message_handler_ == NULL);

  // The validator logs at ERROR level.  Ensure that it generates messages so we
  // can transform them into test failures.
  old_min_log_level_ = logging::GetMinLogLevel();
  if (old_min_log_level_ > kViolationSeverity_)
    logging::SetMinLogLevel(kViolationSeverity_);

  old_message_handler_ = logging::GetLogMessageHandler();
  logging::SetLogMessageHandler(&AddFailureForLogMessage);
}

FailureLogHelper::~FailureLogHelper() {
  logging::SetLogMessageHandler(old_message_handler_);
  old_message_handler_ = NULL;

  if (old_min_log_level_ > kViolationSeverity_)
    logging::SetMinLogLevel(old_min_log_level_);
}

// A logging::LogMessageHandlerFunction that adds a non-fatal test failure
// (i.e., similar to an unmet EXPECT_FOO) for each non-empty message logged at
// the severity of validation violations.  All other messages are sent through
// the default logging pipeline.
// static
bool FailureLogHelper::AddFailureForLogMessage(int severity,
                                               const char* file,
                                               int line,
                                               size_t message_start,
                                               const std::string& str) {
  if (severity == kViolationSeverity_ && !str.empty()) {
    // Remove the trailing newline, if present.
    size_t message_length = str.size() - message_start;
    if (*str.rbegin() == '\n')
      --message_length;
    ADD_FAILURE_AT(file, line)
        << base::StringPiece(str.c_str() + message_start, message_length);
    return true;
  }

  if (old_message_handler_ != NULL)
    return (old_message_handler_)(severity, file, line, message_start, str);

  return false;
}

}  // namespace

InstallationValidator::InstallationType ExpectValidInstallation(
    bool system_level) {
  FailureLogHelper log_helper;
  InstallationValidator::InstallationType found_type =
      InstallationValidator::NO_PRODUCTS;

  EXPECT_TRUE(InstallationValidator::ValidateInstallationType(system_level,
                                                              &found_type));
  return found_type;
}

InstallationValidator::InstallationType ExpectValidInstallationForState(
    const InstallationState& machine_state,
    bool system_level) {
  FailureLogHelper log_helper;
  InstallationValidator::InstallationType found_type =
      InstallationValidator::NO_PRODUCTS;

  EXPECT_TRUE(InstallationValidator::ValidateInstallationTypeForState(
      machine_state, system_level, &found_type));
  return found_type;
}

void ExpectInstallationOfType(bool system_level,
                              InstallationValidator::InstallationType type) {
  EXPECT_EQ(type, ExpectValidInstallation(system_level));
}

void ExpectInstallationOfTypeForState(
    const InstallationState& machine_state,
    bool system_level,
    InstallationValidator::InstallationType type) {
  EXPECT_EQ(type, ExpectValidInstallationForState(machine_state, system_level));
}

}  // namespace installer
