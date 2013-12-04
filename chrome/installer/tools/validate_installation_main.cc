// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A command-line tool that inspects the current system, displaying information
// about installed products.  Violations are dumped to stderr.  The process
// exit code is 0 if there are no violations, or 1 otherwise.

#include <cstdio>
#include <cstdlib>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/installer/util/installation_validator.h"

using installer::InstallationValidator;

namespace {

// A helper class that initializes logging and installs a log message handler to
// direct ERROR messages to stderr.  Only one instance of this class may be live
// at a time.
class ConsoleLogHelper {
 public:
  ConsoleLogHelper();
  ~ConsoleLogHelper();

 private:
  static base::FilePath GetLogFilePath();
  static bool DumpLogMessage(int severity,
                             const char* file,
                             int line,
                             size_t message_start,
                             const std::string& str);

  static const wchar_t kLogFileName_[];
  static FILE* const kOutputStream_;
  static const logging::LogSeverity kViolationSeverity_;
  static logging::LogMessageHandlerFunction old_message_handler_;
  base::FilePath log_file_path_;
};

// static
const wchar_t ConsoleLogHelper::kLogFileName_[] = L"validate_installation.log";

// Dump violations to stderr.
// static
FILE* const ConsoleLogHelper::kOutputStream_ = stderr;

// InstallationValidator logs all violations at ERROR level.
// static
const logging::LogSeverity
    ConsoleLogHelper::kViolationSeverity_ = logging::LOG_ERROR;

// static
logging::LogMessageHandlerFunction
    ConsoleLogHelper::old_message_handler_ = NULL;

ConsoleLogHelper::ConsoleLogHelper() : log_file_path_(GetLogFilePath()) {
  LOG_ASSERT(old_message_handler_ == NULL);
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_FILE;
  settings.log_file = log_file_path_.value().c_str();
  settings.lock_log = logging::DONT_LOCK_LOG_FILE;
  settings.delete_old = logging::DELETE_OLD_LOG_FILE;
  logging::InitLogging(settings);

  old_message_handler_ = logging::GetLogMessageHandler();
  logging::SetLogMessageHandler(&DumpLogMessage);
}

ConsoleLogHelper::~ConsoleLogHelper() {
  logging::SetLogMessageHandler(old_message_handler_);
  old_message_handler_ = NULL;

  logging::CloseLogFile();

  // Delete the log file if it wasn't written to (this is expected).
  int64 file_size = 0;
  if (base::GetFileSize(log_file_path_, &file_size) && file_size == 0)
    base::DeleteFile(log_file_path_, false);
}

// Returns the path to the log file to create.  The file should be empty at
// process exit since we redirect log messages to stderr.
// static
base::FilePath ConsoleLogHelper::GetLogFilePath() {
  base::FilePath log_path;

  if (PathService::Get(base::DIR_TEMP, &log_path))
    return log_path.Append(kLogFileName_);
  else
    return base::FilePath(kLogFileName_);
}

// A logging::LogMessageHandlerFunction that sends the body of messages logged
// at the severity of validation violations to stderr.  All other messages are
// sent through the default logging pipeline.
// static
bool ConsoleLogHelper::DumpLogMessage(int severity,
                                      const char* file,
                                      int line,
                                      size_t message_start,
                                      const std::string& str) {
  if (severity == kViolationSeverity_) {
    fprintf(kOutputStream_, "%s", str.c_str() + message_start);
    return true;
  }

  if (old_message_handler_ != NULL)
    return (old_message_handler_)(severity, file, line, message_start, str);

  return false;
}

const char* LevelToString(bool system_level) {
  return system_level ? "System-level" : "User-level";
}

std::string InstallationTypeToString(
    InstallationValidator::InstallationType type) {
  std::string result;

  static const struct ProductData {
    int bit;
    const char* name;
  } kProdBitToName[] = {
    {
      InstallationValidator::ProductBits::CHROME_SINGLE,
      "Chrome"
    }, {
      InstallationValidator::ProductBits::CHROME_MULTI,
      "Chrome (multi)"
    }, {
      InstallationValidator::ProductBits::CHROME_FRAME_SINGLE,
      "Chrome Frame"
    }, {
      InstallationValidator::ProductBits::CHROME_FRAME_MULTI,
      "Chrome Frame (multi)"
    }, {
      InstallationValidator::ProductBits::CHROME_FRAME_READY_MODE,
      "Ready-mode Chrome Frame"
    },
  };

  for (size_t i = 0; i < arraysize(kProdBitToName); ++i) {
    const ProductData& product_data = kProdBitToName[i];
    if ((type & product_data.bit) != 0) {
      if (!result.empty())
        result.append(", ");
      result.append(product_data.name);
    }
  }

  return result;
}

}  // namespace

// The main program.
int wmain(int argc, wchar_t *argv[]) {
  int result = EXIT_SUCCESS;
  base::AtExitManager exit_manager;

  CommandLine::Init(0, NULL);
  ConsoleLogHelper log_helper;

  // Check user-level and system-level for products.
  for (int i = 0; i < 2; ++i) {
    const bool system_level = (i != 0);
    InstallationValidator::InstallationType type =
        InstallationValidator::NO_PRODUCTS;
    bool is_valid =
        InstallationValidator::ValidateInstallationType(system_level, &type);
    if (type != InstallationValidator::NO_PRODUCTS) {
      FILE* stream = is_valid ? stdout : stderr;
      fprintf(stream, "%s installations%s: %s\n", LevelToString(system_level),
              (is_valid ? "" : " (with errors)"),
              InstallationTypeToString(type).c_str());
    }
    if (!is_valid)
      result = EXIT_FAILURE;
  }

  return result;
}
