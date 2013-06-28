// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/logging.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "chrome/test/chromedriver/capabilities.h"
#include "chrome/test/chromedriver/chrome/console_logger.h"
#include "chrome/test/chromedriver/chrome/performance_logger.h"
#include "chrome/test/chromedriver/chrome/status.h"


namespace {

// Map between WebDriverLog::WebDriverLevel and its name in WD wire protocol.
// Array indices are the WebDriverLog::WebDriverLevel enum values.
const char* const kWebDriverLevelNames[] = {
  "ALL", "DEBUG", "INFO", "WARNING", "SEVERE", "OFF"
};

// Map between Log::Level and WebDriverLog::WebDriverLevel.
// Array indices are the Log::Level enum values.
WebDriverLog::WebDriverLevel kLogLevelToWebDriverLevels[] = {
  WebDriverLog::kWdDebug,  // kDebug
  WebDriverLog::kWdInfo,  // kLog
  WebDriverLog::kWdWarning,  // kWarning
  WebDriverLog::kWdSevere  // kError
};

// Translates Log::Level to WebDriverLog::WebDriverLevel.
WebDriverLog::WebDriverLevel LogLevelToWebDriverLevel(Log::Level level) {
  const int index = level - Log::kDebug;
  CHECK_GE(index, 0);
  CHECK_LT(static_cast<size_t>(index), arraysize(kLogLevelToWebDriverLevels));
  return kLogLevelToWebDriverLevels[index];
}

// Returns WD wire protocol level name for a WebDriverLog::WebDriverLevel.
std::string GetWebDriverLevelName(
    const WebDriverLog::WebDriverLevel level) {
  const int index = level - WebDriverLog::kWdAll;
  CHECK_GE(index, 0);
  CHECK_LT(static_cast<size_t>(index), arraysize(kWebDriverLevelNames));
  return kWebDriverLevelNames[index];
}

}  // namespace

bool WebDriverLog::NameToLevel(
    const std::string& name, WebDriverLog::WebDriverLevel* out_level) {
  for (size_t i = 0; i < arraysize(kWebDriverLevelNames); ++i) {
    if (name == kWebDriverLevelNames[i]) {
      CHECK_LE(WebDriverLog::kWdAll + i,
               static_cast<size_t>(WebDriverLog::kWdOff));
      *out_level =
          static_cast<WebDriverLog::WebDriverLevel>(WebDriverLog::kWdAll + i);
      return true;
    }
  }
  return false;
}

WebDriverLog::WebDriverLog(
    const std::string& type, WebDriverLog::WebDriverLevel min_wd_level)
    : type_(type),
      min_wd_level_(min_wd_level),
      entries_(new base::ListValue()) {
  VLOG(1) << "Log(" << type_ << ", " << min_wd_level_ << ")";
}

WebDriverLog::~WebDriverLog() {
  VLOG(1) << "Log type '" << type_ << "' lost "
          << entries_->GetSize() << " entries on destruction";
}

const std::string& WebDriverLog::GetType() {
  return type_;
}

void WebDriverLog::AddEntryTimestamped(const base::Time& timestamp,
                                       Log::Level level,
                                       const std::string& message) {
  const WebDriverLog::WebDriverLevel wd_level = LogLevelToWebDriverLevel(level);
  if (wd_level < min_wd_level_)
    return;
  scoped_ptr<base::DictionaryValue> log_entry_dict(new base::DictionaryValue());
  log_entry_dict->SetDouble("timestamp",
                            static_cast<int64>(timestamp.ToJsTime()));
  log_entry_dict->SetString("level", GetWebDriverLevelName(wd_level));
  log_entry_dict->SetString("message", message);
  entries_->Append(log_entry_dict.release());
}

scoped_ptr<base::ListValue> WebDriverLog::GetAndClearEntries() {
  scoped_ptr<base::ListValue> ret(entries_.release());
  entries_.reset(new base::ListValue());
  return ret.Pass();
}

Status CreateLogs(const Capabilities& capabilities,
                  ScopedVector<WebDriverLog>* out_devtools_logs,
                  ScopedVector<DevToolsEventListener>* out_listeners) {
  ScopedVector<WebDriverLog> devtools_logs;
  ScopedVector<DevToolsEventListener> listeners;
  WebDriverLog::WebDriverLevel browser_log_level = WebDriverLog::kWdInfo;

  if (capabilities.logging_prefs) {
    for (DictionaryValue::Iterator pref(*capabilities.logging_prefs);
         !pref.IsAtEnd(); pref.Advance()) {
      const std::string type = pref.key();
      std::string level_name;
      if (!pref.value().GetAsString(&level_name)) {
        return Status(kUnknownError,
                      "logging level must be a string for log type: " + type);
      }
      WebDriverLog::WebDriverLevel level = WebDriverLog::kWdOff;
      if (!WebDriverLog::NameToLevel(level_name, &level)) {
        return Status(kUnknownError,
                      "invalid log level \"" + level_name +
                      "\" for type: " + type);
      }
      if ("performance" == type) {
        if (WebDriverLog::kWdOff != level) {
          WebDriverLog* log = new WebDriverLog(type, WebDriverLog::kWdAll);
          devtools_logs.push_back(log);
          listeners.push_back(new PerformanceLogger(log));
        }
      } else if ("browser" == type) {
        browser_log_level = level;
      } else {
        // Driver "should" ignore unrecognized log types, per Selenium tests.
        // For example the Java client passes the "client" log type in the caps,
        // which the server should never provide.
        LOG(WARNING) << "Ignoring unrecognized log type: LoggingPrefs." << type;
      }
    }
  }
  // Create "browser" log -- should always exist.
  WebDriverLog* browser_log = new WebDriverLog("browser", browser_log_level);
  devtools_logs.push_back(browser_log);
  // If the level is OFF, don't even bother listening for DevTools events.
  if (browser_log_level != WebDriverLog::kWdOff)
    listeners.push_back(new ConsoleLogger(browser_log));

  out_devtools_logs->swap(devtools_logs);
  out_listeners->swap(listeners);
  return Status(kOk);
}
