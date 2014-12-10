// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/device_event_log.h"

#include <string>

#include "base/logging.h"
#include "chromeos/device_event_log_impl.h"

namespace chromeos {

namespace device_event_log {

namespace {

const size_t kDefaultMaxEntries = 4000;

DeviceEventLogImpl* g_device_event_log = NULL;

}  // namespace

const LogLevel kDefaultLogLevel = LOG_LEVEL_EVENT;

void Initialize(size_t max_entries) {
  CHECK(!g_device_event_log);
  if (max_entries == 0)
    max_entries = kDefaultMaxEntries;
  g_device_event_log = new DeviceEventLogImpl(max_entries);
}

void Shutdown() {
  delete g_device_event_log;
  g_device_event_log = NULL;
}

void AddEntry(const char* file,
              int line,
              LogType type,
              LogLevel level,
              const std::string& event) {
  if (g_device_event_log) {
    g_device_event_log->AddEntry(file, line, type, level, event);
  } else {
    DeviceEventLogImpl::SendToVLogOrErrorLog(file, line, type, level, event);
  }
}

void AddEntryWithDescription(const char* file,
                             int line,
                             LogType type,
                             LogLevel level,
                             const std::string& event,
                             const std::string& desc) {
  std::string event_with_desc = event;
  if (!desc.empty())
    event_with_desc += ": " + desc;
  AddEntry(file, line, type, level, event_with_desc);
}

std::string GetAsString(StringOrder order,
                        const std::string& format,
                        const std::string& types,
                        LogLevel max_level,
                        size_t max_events) {
  if (!g_device_event_log)
    return "DeviceEventLog not initialized.";
  return g_device_event_log->GetAsString(order, format, types, max_level,
                                         max_events);
}

namespace internal {

DeviceEventLogInstance::DeviceEventLogInstance(const char* file,
                                               int line,
                                               device_event_log::LogType type,
                                               device_event_log::LogLevel level)
    : file_(file), line_(line), type_(type), level_(level) {
}

DeviceEventLogInstance::~DeviceEventLogInstance() {
  device_event_log::AddEntry(file_, line_, type_, level_, stream_.str());
}

}  // namespace internal

}  // namespace device_event_log

}  // namespace chromeos
