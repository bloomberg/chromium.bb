// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/log.h"

#include <stdio.h>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"

namespace {

// Truncates the given string to 200 characters, adding an ellipsis if
// truncation was necessary.
void TruncateString(std::string* data) {
  const size_t kMaxLength = 200;
  if (data->length() > kMaxLength) {
    data->resize(kMaxLength);
    data->replace(kMaxLength - 3, 3, "...");
  }
}

// Truncates all strings contained in the given value.
void TruncateContainedStrings(base::Value* value) {
  base::ListValue* list = NULL;
  base::DictionaryValue* dict = NULL;
  if (value->GetAsDictionary(&dict)) {
    for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd();
         it.Advance()) {
      std::string data;
      if (it.value().GetAsString(&data)) {
        TruncateString(&data);
        dict->SetWithoutPathExpansion(it.key(), new base::StringValue(data));
      } else {
        base::Value* child = NULL;
        dict->GetWithoutPathExpansion(it.key(), &child);
        TruncateContainedStrings(child);
      }
    }
  } else if (value->GetAsList(&list)) {
    for (size_t i = 0; i < list->GetSize(); ++i) {
      base::Value* child = NULL;
      if (!list->Get(i, &child))
        continue;
      std::string data;
      if (child->GetAsString(&data)) {
        TruncateString(&data);
        list->Set(i, new base::StringValue(data));
      } else {
        TruncateContainedStrings(child);
      }
    }
  }
}

std::string ConvertForDisplayInternal(const std::string& input) {
  size_t left = input.find("{");
  size_t right = input.rfind("}");
  if (left == std::string::npos || right == std::string::npos)
    return input.substr(0, 10 << 10);

  scoped_ptr<base::Value> value(
      base::JSONReader::Read(input.substr(left, right - left + 1)));
  if (!value)
    return input.substr(0, 10 << 10);
  TruncateContainedStrings(value.get());
  std::string json;
  base::JSONWriter::WriteWithOptions(
      value.get(), base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  std::string display = input.substr(0, left) + json;
  if (input.length() > right)
    display += input.substr(right + 1);
  return display;
}

// Pretty prints encapsulated JSON and truncates long strings for display.
std::string ConvertForDisplay(const std::string& input) {
  std::string display = ConvertForDisplayInternal(input);
  char remove_chars[] = {'\r', '\0'};
  RemoveChars(display, remove_chars, &display);
  return display;
}

}  // namespace

void Log::AddEntry(Level level, const std::string& message) {
 AddEntryTimestamped(base::Time::Now(), level, message);
}

Logger::Logger() : min_log_level_(kLog), start_(base::Time::Now()) {}

Logger::Logger(Level min_log_level)
    : min_log_level_(min_log_level), start_(base::Time::Now()) {}

Logger::~Logger() {}

void Logger::AddEntryTimestamped(const base::Time& timestamp,
                                 Level level,
                                 const std::string& message) {
  if (level < min_log_level_)
    return;

  const char* level_name = "UNKNOWN";
  switch (level) {
    case kDebug:
      level_name = "DEBUG";
      break;
    case kLog:
      level_name = "INFO";
      break;
    case kWarning:
      level_name = "WARNING";
      break;
    case kError:
      level_name = "ERROR";
      break;
    default:
      break;
  }
  std::string entry =
      base::StringPrintf("[%.3lf][%s]: %s",
                         base::TimeDelta(timestamp - start_).InSecondsF(),
                         level_name,
                         ConvertForDisplay(message).c_str());
  const char* format = "%s\n";
  if (entry[entry.length() - 1] == '\n')
    format = "%s";
  fprintf(stderr, format, entry.c_str());
  fflush(stderr);
}
