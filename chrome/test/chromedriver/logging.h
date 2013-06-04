// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_LOGGING_H_
#define CHROME_TEST_CHROMEDRIVER_LOGGING_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/log.h"

struct Capabilities;
class DevToolsEventListener;
class ListValue;
class Status;

// Accumulates WebDriver Logging API entries of a given type and minimum level.
// See https://code.google.com/p/selenium/wiki/Logging.
class WebDriverLog : public Log {
 public:
  // Constants corresponding to log entry severity levels in the wire protocol.
  enum WebDriverLevel {
    kWdAll,
    kWdDebug,
    kWdInfo,
    kWdWarning,
    kWdSevere,
    kWdOff
  };

  // Converts WD wire protocol level name -> WebDriverLevel, false on bad name.
  static bool NameToLevel(const std::string& name, WebDriverLevel* out_level);

  // Creates a WebDriverLog with the given type and minimum level.
  WebDriverLog(const std::string& type, WebDriverLevel min_wd_level);
  virtual ~WebDriverLog();

  // Returns this log's type, for the WD wire protocol "/log" and "/log/types".
  const std::string& GetType();

  // Returns entries accumulated so far, as a ListValue ready for serialization
  // into the wire protocol response to the "/log" command.
  // The caller assumes ownership of the ListValue, and the WebDriverLog
  // creates and owns a new empty ListValue for further accumulation.
  scoped_ptr<base::ListValue> GetAndClearEntries();

  // Translates a Log entry level into a WebDriver level and stores the entry.
  virtual void AddEntryTimestamped(const base::Time& timestamp,
                                   Level level,
                                   const std::string& message) OVERRIDE;

 private:
  const std::string type_;  // WebDriver log type.
  const WebDriverLevel min_wd_level_;  // Minimum level of entries to store.
  scoped_ptr<base::ListValue> entries_;  // Accumulated entries.

  DISALLOW_COPY_AND_ASSIGN(WebDriverLog);
};

// Creates Log's and DevToolsEventListener's for ones that are DevTools-based.
Status CreateLogs(const Capabilities& capabilities,
                  ScopedVector<WebDriverLog>* out_devtools_logs,
                  ScopedVector<DevToolsEventListener>* out_listeners);

#endif  // CHROME_TEST_CHROMEDRIVER_LOGGING_H_
