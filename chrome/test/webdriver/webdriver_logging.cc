// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/webdriver_logging.h"

#include <cmath>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/time.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;

namespace webdriver {

FileLog* FileLog::singleton_ = NULL;

double start_time = 0;

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
void FileLog::InitGlobalLog(LogLevel level) {
  singleton_ = new FileLog(FilePath(FILE_PATH_LITERAL("chromedriver.log")),
                           level);
}

// static
FileLog* FileLog::Get() {
  return singleton_;
}

FileLog::FileLog(const FilePath& path, LogLevel level)
    : path_(path),
      min_log_level_(level) {
  file_.reset(file_util::OpenFile(path, "w"));
}

FileLog::~FileLog() { }

void FileLog::Log(LogLevel level, const base::Time& time,
                  const std::string& message) {
  if (level < min_log_level_)
    return;

  const char* level_name = "UNKNOWN";
  switch (level) {
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
  base::TimeDelta delta(time - base::Time::UnixEpoch());
  std::string time_utc = base::Int64ToString(delta.InMilliseconds());
  std::string entry = base::StringPrintf(
      "[%s][%s]:", time_utc.c_str(), level_name);

  int pad_length = 26 - entry.length();
  if (pad_length < 1)
    pad_length = 1;
  std::string padding(pad_length, ' ');
  entry += base::StringPrintf(
      "%s%s\n", padding.c_str(), message.c_str());

  base::AutoLock auto_lock(lock_);
#if defined(OS_WIN)
  SetFilePointer(file_.get(), 0, 0, SEEK_END);
  DWORD num_written;
  WriteFile(file_.get(),
            static_cast<const void*>(entry.c_str()),
            static_cast<DWORD>(entry.length()),
            &num_written,
            NULL);
#else
  fprintf(file_.get(), "%s", entry.c_str());
  fflush(file_.get());
#endif
}

bool FileLog::GetLogContents(std::string* contents) {
  return file_util::ReadFileToString(path_, contents);
}

void FileLog::set_min_log_level(LogLevel level) {
  min_log_level_ = level;
}

InMemoryLog::InMemoryLog() { }

InMemoryLog::~InMemoryLog() {  }

void InMemoryLog::Log(LogLevel level, const base::Time& time,
                      const std::string& message) {
  // base's JSONWriter doesn't obey the spec, and writes
  // doubles without a fraction with a '.0' postfix. base's Value class
  // only includes doubles or int types. Instead of returning the timestamp
  // in unix epoch time, we return it based on the start of chromedriver so
  // a 32-bit int doesn't overflow.
  // TODO(kkania): Add int64_t to base Values or fix the JSONWriter/Reader and
  // use unix epoch time.
  base::TimeDelta delta(time - base::Time::FromDoubleT(start_time));
  DictionaryValue* entry = new DictionaryValue();
  entry->SetInteger("level", level);
  entry->SetInteger("timestamp", delta.InMilliseconds());
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

void InitWebDriverLogging(LogLevel min_log_level) {
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
  FileLog::InitGlobalLog(min_log_level);
}

}  // namespace webdriver
