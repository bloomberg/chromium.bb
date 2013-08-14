// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_event_log.h"

#include <list>

#include "base/files/file_path.h"
#include "base/i18n/time_formatting.h"
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
namespace network_event_log {

namespace {

class NetworkEventLog;
NetworkEventLog* g_network_event_log = NULL;
size_t g_max_network_event_log_entries = 4000;
const char* kLogLevelName[] = {"Error", "User", "Event", "Debug"};

struct LogEntry {
  LogEntry(const std::string& file,
           int file_line,
           LogLevel log_level,
           const std::string& event,
           const std::string& description);

  std::string ToString(bool show_time,
                       bool show_file,
                       bool show_desc,
                       bool format_html) const;
  void ToDictionary(base::DictionaryValue*) const;

  std::string GetNormalText(bool show_desc) const;
  std::string GetHtmlText(bool show_desc) const;
  std::string GetAsJSON() const;

  void SendToVLogOrErrorLog() const;

  bool ContentEquals(const LogEntry& other) const;

  std::string file;
  int file_line;
  LogLevel log_level;
  std::string event;
  std::string description;
  base::Time time;
  int count;
};

LogEntry::LogEntry(const std::string& file,
                   int file_line,
                   LogLevel log_level,
                   const std::string& event,
                   const std::string& description)
    : file(file),
      file_line(file_line),
      log_level(log_level),
      event(event),
      description(description),
      time(base::Time::Now()),
      count(1) {}

std::string LogEntry::ToString(bool show_time,
                               bool show_file,
                               bool show_desc,
                               bool format_html) const {
  std::string line;
  if (show_time)
    line += "[" + UTF16ToUTF8(base::TimeFormatTimeOfDay(time)) + "] ";
  if (show_file) {
    std::string filestr = format_html ? net::EscapeForHTML(file) : file;
    line += base::StringPrintf("%s:%d ", file.c_str(), file_line);
  }
  line += format_html ? GetHtmlText(show_desc) : GetNormalText(show_desc);
  if (count > 1)
    line += base::StringPrintf(" (%d)", count);
  return line;
}

void LogEntry::ToDictionary(base::DictionaryValue* output) const {
  output->SetString("timestamp", base::TimeFormatShortDateAndTime(time));
  output->SetString("level", kLogLevelName[log_level]);
  output->SetString("file",
                    base::StringPrintf("%s:%d ", file.c_str(), file_line));
  output->SetString("event", event);
  output->SetString("description", description);
}

std::string LogEntry::GetAsJSON() const {
  base::DictionaryValue entry;
  ToDictionary(&entry);
  std::string json;
  JSONStringValueSerializer serializer(&json);
  if (!serializer.Serialize(entry)) {
    LOG(ERROR) << "Failed to serialize to JSON";
  }
  return json;
}

std::string LogEntry::GetNormalText(bool show_desc) const {
  std::string text = event;
  if (show_desc && !description.empty())
    text += ": " + description;
  return text;
}

std::string LogEntry::GetHtmlText(bool show_desc) const {
  std::string text;
  if (log_level == LOG_LEVEL_DEBUG)
    text += "<i>";
  else if (log_level == LOG_LEVEL_USER)
    text += "<b>";
  else if (log_level == LOG_LEVEL_ERROR)
    text += "<b><i>";

  text += event;
  if (show_desc && !description.empty())
    text += ": " + net::EscapeForHTML(description);

  if (log_level == LOG_LEVEL_DEBUG)
    text += "</i>";
  else if (log_level == LOG_LEVEL_USER)
    text += "</b>";
  else if (log_level == LOG_LEVEL_ERROR)
    text += "</i></b>";
  return text;
}

void LogEntry::SendToVLogOrErrorLog() const {
  const bool show_time = true;
  const bool show_file = true;
  const bool show_desc = true;
  const bool format_html = false;
  std::string output = ToString(show_time, show_file, show_desc, format_html);
  if (log_level == LOG_LEVEL_ERROR)
    LOG(ERROR) << output;
  else
    VLOG(1) << output;
}

bool LogEntry::ContentEquals(const LogEntry& other) const {
  return file == other.file &&
         file_line == other.file_line &&
         event == other.event &&
         description == other.description;
}

void GetFormat(const std::string& format_string,
               bool* show_time,
               bool* show_file,
               bool* show_desc,
               bool* format_html,
               bool* format_json) {
  base::StringTokenizer tokens(format_string, ",");
  *show_time = false;
  *show_file = false;
  *show_desc = false;
  *format_html = false;
  *format_json = false;
  while (tokens.GetNext()) {
    std::string tok(tokens.token());
    if (tok == "time")
      *show_time = true;
    if (tok == "file")
      *show_file = true;
    if (tok == "desc")
      *show_desc = true;
    if (tok == "html")
      *format_html = true;
    if (tok == "json")
      *format_json = true;
  }
}

typedef std::list<LogEntry> LogEntryList;

class NetworkEventLog {
 public:
  NetworkEventLog() {}
  ~NetworkEventLog() {}

  void AddLogEntry(const LogEntry& entry);

  std::string GetAsString(StringOrder order,
                          const std::string& format,
                          LogLevel max_level,
                          size_t max_events);

  LogEntryList& entries() { return entries_; }

 private:
  LogEntryList entries_;

  DISALLOW_COPY_AND_ASSIGN(NetworkEventLog);
};

void NetworkEventLog::AddLogEntry(const LogEntry& entry) {
  if (!entries_.empty()) {
    LogEntry& last = entries_.back();
    if (last.ContentEquals(entry)) {
      // Update count and time for identical events to avoid log spam.
      ++last.count;
      last.log_level = std::min(last.log_level, entry.log_level);
      last.time = base::Time::Now();
      return;
    }
  }
  if (entries_.size() >= g_max_network_event_log_entries) {
    const size_t max_error_entries = g_max_network_event_log_entries / 2;
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
  entry.SendToVLogOrErrorLog();
}

std::string NetworkEventLog::GetAsString(StringOrder order,
                                         const std::string& format,
                                         LogLevel max_level,
                                         size_t max_events) {
  if (entries_.empty())
    return "No Log Entries.";

  bool show_time, show_file, show_desc, format_html, format_json;
  GetFormat(
      format, &show_time, &show_file, &show_desc, &format_html, &format_json);

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
           riter != entries_.rend();
           ++riter) {
        ++num_entries;
        if (riter->log_level > max_level)
          continue;
        if (++shown_events >= max_events)
          break;
      }
      offset = entries_.size() - num_entries;
    }
    for (LogEntryList::const_iterator iter = entries_.begin();
         iter != entries_.end();
         ++iter) {
      if (offset > 0) {
        --offset;
        continue;
      }
      if (iter->log_level > max_level)
        continue;
      if (format_json) {
        log_entries.AppendString((*iter).GetAsJSON());
      } else {
        result +=
            (*iter).ToString(show_time, show_file, show_desc, format_html);
        result += "\n";
      }
    }
  } else {
    size_t nlines = 0;
    // Iterate backwards through the list to show the most recent entries first.
    for (LogEntryList::const_reverse_iterator riter = entries_.rbegin();
         riter != entries_.rend();
         ++riter) {
      if (riter->log_level > max_level)
        continue;
      if (format_json) {
        log_entries.AppendString((*riter).GetAsJSON());
      } else {
        result +=
            (*riter).ToString(show_time, show_file, show_desc, format_html);
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

}  // namespace

const LogLevel kDefaultLogLevel = LOG_LEVEL_EVENT;

void Initialize() {
  if (g_network_event_log)
    delete g_network_event_log;  // reset log
  g_network_event_log = new NetworkEventLog();
}

void Shutdown() {
  delete g_network_event_log;
  g_network_event_log = NULL;
}

bool IsInitialized() { return g_network_event_log != NULL; }

namespace internal {

size_t GetMaxLogEntries() { return g_max_network_event_log_entries; }

void SetMaxLogEntries(size_t max_entries) {
  g_max_network_event_log_entries = max_entries;
  if (!g_network_event_log)
    return;
  while (g_network_event_log->entries().size() > max_entries)
    g_network_event_log->entries().pop_front();
}

void AddEntry(const char* file,
              int file_line,
              LogLevel log_level,
              const std::string& event,
              const std::string& description) {
  std::string filestr;
  if (file)
    filestr = base::FilePath(std::string(file)).BaseName().value();
  LogEntry entry(filestr, file_line, log_level, event, description);
  if (!g_network_event_log) {
    entry.SendToVLogOrErrorLog();
    return;
  }
  g_network_event_log->AddLogEntry(entry);
}

}  // namespace internal

std::string GetAsString(StringOrder order,
                        const std::string& format,
                        LogLevel max_level,
                        size_t max_events) {
  if (!g_network_event_log)
    return "NetworkEventLog not initialized.";
  return g_network_event_log->GetAsString(order, format, max_level, max_events);
}

std::string ValueAsString(const base::Value& value) {
  std::string vstr;
  base::JSONWriter::WriteWithOptions(
      &value, base::JSONWriter::OPTIONS_OMIT_BINARY_VALUES, &vstr);
  return vstr.empty() ? "''" : vstr;
}

}  // namespace network_event_log
}  // namespace chromeos
