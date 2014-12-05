// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/device_event_log_impl.h"

#include <cmath>
#include <list>

#include "base/files/file_path.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "net/base/escape.h"

namespace chromeos {

namespace device_event_log {

namespace {

const char* kLogLevelName[] = {"Error", "User", "Event", "Debug"};

const char* kLogTypeNetworkDesc = "Network";
const char* kLogTypePowerDesc = "Power";

std::string GetLogTypeString(LogType type) {
  if (type == LOG_TYPE_NETWORK)
    return kLogTypeNetworkDesc;
  if (type == LOG_TYPE_POWER)
    return kLogTypePowerDesc;
  NOTREACHED();
  return "Unknown";
}

std::string DateAndTimeWithMicroseconds(const base::Time& time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  // base::Time::Exploded does not include microseconds, but sometimes we need
  // microseconds, so append '.' + usecs to the end of the formatted string.
  int usecs = static_cast<int>(fmod(time.ToDoubleT() * 1000000, 1000000));
  return base::StringPrintf("%04d/%02d/%02d %02d:%02d:%02d.%06d", exploded.year,
                            exploded.month, exploded.day_of_month,
                            exploded.hour, exploded.minute, exploded.second,
                            usecs);
}

std::string TimeWithSeconds(const base::Time& time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  return base::StringPrintf("%02d:%02d:%02d", exploded.hour, exploded.minute,
                            exploded.second);
}

std::string TimeWithMillieconds(const base::Time& time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  return base::StringPrintf("%02d:%02d:%02d.%03d", exploded.hour,
                            exploded.minute, exploded.second,
                            exploded.millisecond);
}

// Defined below for easier review. TODO(stevenjb): Move implementation here.
std::string GetHtmlText(LogLevel log_level, const std::string& event);

std::string LogEntryToString(const DeviceEventLogImpl::LogEntry& log_entry,
                             bool show_time,
                             bool show_file,
                             bool show_type,
                             bool show_level,
                             bool format_html) {
  std::string line;
  if (show_time)
    line += "[" + TimeWithMillieconds(log_entry.time) + "] ";
  if (show_type)
    line += GetLogTypeString(log_entry.log_type) + ": ";
  if (show_level) {
    const char* kLevelDesc[] = {"ERROR", "USER", "EVENT", "DEBUG"};
    line += base::StringPrintf("%s: ", kLevelDesc[log_entry.log_level]);
  }
  if (show_file) {
    std::string filestr =
        format_html ? net::EscapeForHTML(log_entry.file) : log_entry.file;
    line += base::StringPrintf("%s:%d ", log_entry.file.c_str(),
                               log_entry.file_line);
  }
  line += format_html ? GetHtmlText(log_entry.log_level, log_entry.event)
                      : log_entry.event;
  if (log_entry.count > 1)
    line += base::StringPrintf(" (%d)", log_entry.count);
  return line;
}

void LogEntryToDictionary(const DeviceEventLogImpl::LogEntry& log_entry,
                          base::DictionaryValue* output) {
  output->SetString("timestamp", DateAndTimeWithMicroseconds(log_entry.time));
  output->SetString("timestampshort", TimeWithSeconds(log_entry.time));
  output->SetString("level", kLogLevelName[log_entry.log_level]);
  output->SetString("type", GetLogTypeString(log_entry.log_type));
  output->SetString("file", base::StringPrintf("%s:%d ", log_entry.file.c_str(),
                                               log_entry.file_line));
  output->SetString("event", log_entry.event);
}

std::string LogEntryAsJSON(const DeviceEventLogImpl::LogEntry& log_entry) {
  base::DictionaryValue entry_dict;
  LogEntryToDictionary(log_entry, &entry_dict);
  std::string json;
  JSONStringValueSerializer serializer(&json);
  if (!serializer.Serialize(entry_dict)) {
    LOG(ERROR) << "Failed to serialize to JSON";
  }
  return json;
}

std::string GetHtmlText(LogLevel log_level, const std::string& event) {
  std::string text;
  if (log_level == LOG_LEVEL_DEBUG)
    text += "<i>";
  else if (log_level == LOG_LEVEL_USER)
    text += "<b>";
  else if (log_level == LOG_LEVEL_ERROR)
    text += "<b><i>";

  text += net::EscapeForHTML(event);

  if (log_level == LOG_LEVEL_DEBUG)
    text += "</i>";
  else if (log_level == LOG_LEVEL_USER)
    text += "</b>";
  else if (log_level == LOG_LEVEL_ERROR)
    text += "</i></b>";
  return text;
}

void SendLogEntryToVLogOrErrorLog(
    const DeviceEventLogImpl::LogEntry& log_entry) {
  if (log_entry.log_level != LOG_LEVEL_ERROR && !VLOG_IS_ON(1))
    return;
  const bool show_time = true;
  const bool show_file = true;
  const bool show_type = true;
  const bool show_level = false;
  const bool format_html = false;
  std::string output = LogEntryToString(log_entry, show_time, show_file,
                                        show_type, show_level, format_html);
  if (log_entry.log_level == LOG_LEVEL_ERROR)
    LOG(ERROR) << output;
  else
    VLOG(1) << output;
}

bool LogEntryMatches(const DeviceEventLogImpl::LogEntry& first,
                     const DeviceEventLogImpl::LogEntry& second) {
  return first.file == second.file && first.file_line == second.file_line &&
         first.log_level == second.log_level &&
         first.log_type == second.log_type && first.event == second.event;
}

bool LogEntryMatchesType(const DeviceEventLogImpl::LogEntry& entry,
                         LogType type) {
  if (type == LOG_TYPE_ALL)
    return true;
  if (type == LOG_TYPE_NON_NETWORK && entry.log_type != LOG_TYPE_NETWORK)
    return true;
  return type == entry.log_type;
}

void GetFormat(const std::string& format_string,
               bool* show_time,
               bool* show_file,
               bool* show_type,
               bool* show_level,
               bool* format_html,
               bool* format_json) {
  base::StringTokenizer tokens(format_string, ",");
  *show_time = false;
  *show_file = false;
  *show_type = false;
  *show_level = false;
  *format_html = false;
  *format_json = false;
  while (tokens.GetNext()) {
    std::string tok(tokens.token());
    if (tok == "time")
      *show_time = true;
    if (tok == "file")
      *show_file = true;
    if (tok == "type")
      *show_type = true;
    if (tok == "level")
      *show_level = true;
    if (tok == "html")
      *format_html = true;
    if (tok == "json")
      *format_json = true;
  }
}

}  // namespace

// static
void DeviceEventLogImpl::SendToVLogOrErrorLog(const char* file,
                                              int file_line,
                                              LogType log_type,
                                              LogLevel log_level,
                                              const std::string& event) {
  LogEntry entry(file, file_line, log_type, log_level, event);
  SendLogEntryToVLogOrErrorLog(entry);
}

DeviceEventLogImpl::DeviceEventLogImpl(size_t max_entries)
    : max_entries_(max_entries) {
}

DeviceEventLogImpl::~DeviceEventLogImpl() {
}

void DeviceEventLogImpl::AddEntry(const char* file,
                                  int file_line,
                                  LogType log_type,
                                  LogLevel log_level,
                                  const std::string& event) {
  LogEntry entry(file, file_line, log_type, log_level, event);
  AddLogEntry(entry);
}

void DeviceEventLogImpl::AddLogEntry(const LogEntry& entry) {
  if (!entries_.empty()) {
    LogEntry& last = entries_.back();
    if (LogEntryMatches(last, entry)) {
      // Update count and time for identical events to avoid log spam.
      ++last.count;
      last.log_level = std::min(last.log_level, entry.log_level);
      last.time = base::Time::Now();
      return;
    }
  }
  if (entries_.size() >= max_entries_) {
    const size_t max_error_entries = max_entries_ / 2;
    // Remove the first (oldest) non-error entry, or the oldest entry if more
    // than half the entries are errors.
    size_t error_count = 0;
    for (LogEntryList::iterator iter = entries_.begin(); iter != entries_.end();
         ++iter) {
      if (iter->log_level != LOG_LEVEL_ERROR) {
        entries_.erase(iter);
        break;
      }
      if (++error_count > max_error_entries) {
        // Too many error entries, remove the oldest entry.
        entries_.pop_front();
        break;
      }
    }
  }
  entries_.push_back(entry);
  SendLogEntryToVLogOrErrorLog(entry);
}

std::string DeviceEventLogImpl::GetAsString(StringOrder order,
                                            const std::string& format,
                                            LogType log_type,
                                            LogLevel max_level,
                                            size_t max_events) {
  if (entries_.empty())
    return "No Log Entries.";

  bool show_time, show_file, show_type, show_level, format_html, format_json;
  GetFormat(format, &show_time, &show_file, &show_type, &show_level,
            &format_html, &format_json);

  std::string result;
  base::ListValue log_entries;
  if (order == OLDEST_FIRST) {
    size_t offset = 0;
    if (max_events > 0 && max_events < entries_.size()) {
      // Iterate backwards through the list skipping uninteresting entries to
      // determine the first entry to include.
      size_t shown_events = 0;
      size_t num_entries = 0;
      for (LogEntryList::const_reverse_iterator riter = entries_.rbegin();
           riter != entries_.rend(); ++riter) {
        ++num_entries;
        if (!LogEntryMatchesType(*riter, log_type))
          continue;
        if (riter->log_level > max_level)
          continue;
        if (++shown_events >= max_events)
          break;
      }
      offset = entries_.size() - num_entries;
    }
    for (LogEntryList::const_iterator iter = entries_.begin();
         iter != entries_.end(); ++iter) {
      if (offset > 0) {
        --offset;
        continue;
      }
      if (!LogEntryMatchesType(*iter, log_type))
        continue;
      if (iter->log_level > max_level)
        continue;
      if (format_json) {
        log_entries.AppendString(LogEntryAsJSON(*iter));
      } else {
        result += LogEntryToString(*iter, show_time, show_file, show_type,
                                   show_level, format_html);
        result += "\n";
      }
    }
  } else {
    size_t nlines = 0;
    // Iterate backwards through the list to show the most recent entries first.
    for (LogEntryList::const_reverse_iterator riter = entries_.rbegin();
         riter != entries_.rend(); ++riter) {
      if (!LogEntryMatchesType(*riter, log_type))
        continue;
      if (riter->log_level > max_level)
        continue;
      if (format_json) {
        log_entries.AppendString(LogEntryAsJSON(*riter));
      } else {
        result += LogEntryToString(*riter, show_time, show_file, show_type,
                                   show_level, format_html);
        result += "\n";
      }
      if (max_events > 0 && ++nlines >= max_events)
        break;
    }
  }
  if (format_json) {
    JSONStringValueSerializer serializer(&result);
    serializer.Serialize(log_entries);
  }

  return result;
}

DeviceEventLogImpl::LogEntry::LogEntry(const char* filedesc,
                                       int file_line,
                                       LogType log_type,
                                       LogLevel log_level,
                                       const std::string& event)
    : file_line(file_line),
      log_type(log_type),
      log_level(log_level),
      event(event),
      time(base::Time::Now()),
      count(1) {
  if (filedesc)
    file = base::FilePath(std::string(filedesc)).BaseName().value();
}

}  // namespace device_event_log

}  // namespace chromeos
