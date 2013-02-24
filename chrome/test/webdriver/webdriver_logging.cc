// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/webdriver_logging.h"

#include <cmath>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/time.h"
#include "build/build_config.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;

namespace webdriver {

FileLog* FileLog::singleton_ = NULL;

double start_time = 0;

LogLevel LogLevelFromString(const std::string& name) {
  // Default logging level is INFO.
  LogLevel level = kInfoLogLevel;
  const std::string upper_case_name = StringToUpperASCII(name);
  if (upper_case_name == "OFF") {
    level = kOffLogLevel;
  } else if (upper_case_name == "SEVERE") {
    level = kSevereLogLevel;
  } else if (upper_case_name == "WARNING") {
    level = kWarningLogLevel;
  } else if (upper_case_name == "INFO" || upper_case_name == "CONFIG") {

  } else if (upper_case_name == "FINE") {
    level = kFineLogLevel;
  } else if (upper_case_name == "FINER") {
    level = kFinerLogLevel;
  } else if (upper_case_name == "ALL" || upper_case_name == "FINEST") {
    level = kAllLogLevel;
  }
  return level;
}

// static
bool LogType::FromString(const std::string& name, LogType* log_type) {
  if (name == "driver") {
    *log_type = LogType(kDriver);
  } else {
    return false;
  }
  return true;
}

LogType::LogType() : type_(kInvalid) { }

LogType::LogType(Type type) : type_(type) { }

LogType::~LogType() { }

std::string LogType::ToString() const {
  switch (type_) {
    case kDriver:
      return "driver";
    default:
      return "unknown";
  };
}

LogType::Type LogType::type() const {
  return type_;
}

LogHandler::LogHandler() { }

LogHandler::~LogHandler() { }

// static
void FileLog::SetGlobalLog(FileLog* log) {
  singleton_ = log;
}

// static
FileLog* FileLog::Get() {
  return singleton_;
}

// static
FileLog* FileLog::CreateFileLog(const base::FilePath::StringType& log_name,
                                LogLevel level) {
  base::FilePath log_path(log_name);
  file_util::ScopedFILE file(file_util::OpenFile(log_path, "w"));
  base::FilePath temp_dir;
  if (!file.get() && file_util::GetTempDir(&temp_dir))
    log_path = temp_dir.Append(log_name);
  file.reset();
  return new FileLog(log_path, level);
}

FileLog::FileLog(const base::FilePath& path, LogLevel level)
    : path_(path),
      min_log_level_(level) {
  if (!path_.IsAbsolute()) {
    base::FilePath cwd;
    if (file_util::GetCurrentDirectory(&cwd))
      path_ = cwd.Append(path_);
  }
  file_.reset(file_util::OpenFile(path_, "w"));
}

FileLog::~FileLog() { }

void FileLog::Log(LogLevel level, const base::Time& time,
                  const std::string& message) {
  if (!file_.get() || level < min_log_level_)
    return;

  const char* level_name = "UNKNOWN";
  switch (level) {
    case kOffLogLevel:
      level_name = "OFF";
      break;
    case kSevereLogLevel:
      level_name = "SEVERE";
      break;
    case kWarningLogLevel:
      level_name = "WARNING";
      break;
    case kInfoLogLevel:
      level_name = "INFO";
      break;
    case kFineLogLevel:
      level_name = "FINE";
      break;
    case kFinerLogLevel:
      level_name = "FINER";
      break;
    default:
      break;
  }
  base::TimeDelta delta(time - base::Time::FromDoubleT(start_time));
  std::string entry = base::StringPrintf(
      "[%.3lf][%s]:", delta.InSecondsF(), level_name);

  int pad_length = 20 - entry.length();
  if (pad_length < 1)
    pad_length = 1;
  std::string padding(pad_length, ' ');
  entry += padding + message;
#if defined(OS_WIN)
  entry += "\r\n";
#else
  entry += "\n";
#endif

  base::AutoLock auto_lock(lock_);
  fprintf(file_.get(), "%s", entry.c_str());
  fflush(file_.get());
}

bool FileLog::GetLogContents(std::string* contents) const {
  if (!file_.get())
    return false;
  return file_util::ReadFileToString(path_, contents);
}

bool FileLog::IsOpen() const {
  return !!file_.get();
}

void FileLog::set_min_log_level(LogLevel level) {
  min_log_level_ = level;
}

const base::FilePath& FileLog::path() const {
  return path_;
}

InMemoryLog::InMemoryLog() { }

InMemoryLog::~InMemoryLog() {  }

void InMemoryLog::Log(LogLevel level, const base::Time& time,
                      const std::string& message) {
  base::TimeDelta delta = time - base::Time::UnixEpoch();
  DictionaryValue* entry = new DictionaryValue();
  entry->SetInteger("level", level);
  entry->SetDouble("timestamp", std::floor(delta.InMillisecondsF()));
  entry->SetString("message", message);
  base::AutoLock auto_lock(entries_lock_);
  entries_list_.Append(entry);
}

const ListValue* InMemoryLog::entries_list() const {
  return &entries_list_;
}

Logger::Logger() : min_log_level_(kAllLogLevel) { }

Logger::Logger(LogLevel level) : min_log_level_(level) { }

Logger::~Logger() { }

void Logger::Log(LogLevel level, const std::string& message) const {
  if (level < min_log_level_)
    return;

  base::Time time = base::Time::Now();
  for (size_t i = 0; i < handlers_.size(); ++i) {
    handlers_[i]->Log(level, time, message);
  }
}

void Logger::AddHandler(LogHandler* log_handler) {
  handlers_.push_back(log_handler);
}

void Logger::set_min_log_level(LogLevel level) {
  min_log_level_ = level;
}

bool InitWebDriverLogging(const base::FilePath& log_path,
                          LogLevel min_log_level) {
  start_time = base::Time::Now().ToDoubleT();
  // Turn off base/logging.
  bool success = InitLogging(
      NULL,
      logging::LOG_NONE,
      logging::DONT_LOCK_LOG_FILE,
      logging::DELETE_OLD_LOG_FILE,
      logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);
  if (!success) {
    PLOG(ERROR) << "Unable to initialize logging";
  }
  logging::SetLogItems(false,  // enable_process_id
                       false,  // enable_thread_id
                       true,   // enable_timestamp
                       false); // enable_tickcount

  // Init global file log.
  FileLog* log;
  if (log_path.empty()) {
    log = FileLog::CreateFileLog(FILE_PATH_LITERAL("chromedriver.log"),
                                 min_log_level);
  } else {
    log = new FileLog(log_path, min_log_level);
  }
  FileLog::SetGlobalLog(log);
  return log->IsOpen();
}

}  // namespace webdriver
