// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_event_log.h"

#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"

namespace chromeos {

namespace network_event_log {

namespace {

struct LogEntry {
  LogEntry(const std::string& module,
           const std::string& event,
           const std::string& description);

  std::string ToString() const;

  bool ContentEquals(const LogEntry& other) const;

  std::string module;
  std::string event;
  std::string description;
  base::Time time;
  int count;
};

LogEntry::LogEntry(const std::string& module,
                   const std::string& event,
                   const std::string& description)
    : module(module),
      event(event),
      description(description),
      time(base::Time::Now()),
      count(1) {
}

std::string LogEntry::ToString() const {
  std::string line;
  line += "[" + UTF16ToUTF8(base::TimeFormatShortDateAndTime(time)) + "]";
  line += " " + module + ":" + event;
  if (!description.empty())
    line += ": " + description;
  if (count > 1)
    line += StringPrintf(" (%d)", count);
  line += "\n";
  return line;
}

bool LogEntry::ContentEquals(const LogEntry& other) const {
  return module == other.module &&
      event == other.event &&
      description == other.description;
}

typedef std::deque<LogEntry> LogEntryList;

class NetworkEventLog {
 public:
  NetworkEventLog() {}
  ~NetworkEventLog() {}

  void AddEntry(const LogEntry& entry);

  std::string GetAsString(StringOrder order,
                          size_t max_events);

 private:
  LogEntryList entries_;

  DISALLOW_COPY_AND_ASSIGN(NetworkEventLog);
};

void NetworkEventLog::AddEntry(const LogEntry& entry) {
  if (!entries_.empty()) {
    LogEntry& last = entries_.back();
    if (last.ContentEquals(entry)) {
      // Update count and time for identical events to avoid log spam.
      ++last.count;
      last.time = base::Time::Now();
      return;
    }
  }
  if (entries_.size() >= kMaxNetworkEventLogEntries)
    entries_.pop_front();
  entries_.push_back(entry);
  VLOG(2) << entry.ToString();
}

std::string NetworkEventLog::GetAsString(StringOrder order,
                                         size_t max_events) {
  if (entries_.empty())
    return "No Log Entries.";

  std::string result;
  if (order == OLDEST_FIRST) {
    size_t offset = 0;
    if (max_events > 0 && max_events < entries_.size())
      offset = entries_.size() - max_events;
    for (LogEntryList::const_iterator iter = entries_.begin() + offset;
         iter != entries_.end(); ++iter) {
      result += (*iter).ToString();
    }
  } else {
    size_t nlines = 0;
    // Iterate backwards through the list to show the most recent entries first.
    for (LogEntryList::const_reverse_iterator riter = entries_.rbegin();
         riter != entries_.rend(); ++riter) {
      result += (*riter).ToString();
      if (max_events > 0 && ++nlines >= max_events)
        break;
    }
  }
  return result;
}

}  // namespace

NetworkEventLog* g_network_event_log = NULL;
const size_t kMaxNetworkEventLogEntries = 1000;

void Initialize() {
  if (g_network_event_log)
    delete g_network_event_log;  // reset log
  g_network_event_log = new NetworkEventLog();
}

void Shutdown() {
  delete g_network_event_log;
  g_network_event_log = NULL;
}

bool IsInitialized() {
  return g_network_event_log != NULL;
}

void AddEntry(const std::string& module,
              const std::string& event,
              const std::string& description) {
  LogEntry entry(module, event, description);
  if (!g_network_event_log) {
    VLOG(2) << entry.ToString();
    return;
  }
  g_network_event_log->AddEntry(entry);
}

std::string GetAsString(StringOrder order, size_t max_events) {
  if (!g_network_event_log)
    return "NetworkEventLog not intitialized.";
  return g_network_event_log->GetAsString(order, max_events);
}

}  // namespace network_event_log

}  // namespace chromeos
