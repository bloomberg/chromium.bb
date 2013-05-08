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

// Accumulates WebDriver Logging API events of a given type and minimum level.
// See https://code.google.com/p/selenium/wiki/Logging.
class WebDriverLog : public Log {
 public:
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

  WebDriverLog(const std::string& type, WebDriverLevel min_wd_level);
  virtual ~WebDriverLog();

  const std::string& GetType();

  scoped_ptr<base::ListValue> GetAndClearEntries();

  virtual void AddEntry(const base::Time& time,
                        Level level,
                        const std::string& message) OVERRIDE;
  using Log::AddEntry;  // Inherited overload that takes level and message.

 private:
  const std::string type_;
  const WebDriverLevel min_wd_level_;
  scoped_ptr<base::ListValue> entries_;

  DISALLOW_COPY_AND_ASSIGN(WebDriverLog);
};

// Creates Log's and DevToolsEventListener's for ones that are DevTools-based.
Status CreateLogs(const Capabilities& capabilities,
                  ScopedVector<WebDriverLog>* out_devtools_logs,
                  ScopedVector<DevToolsEventListener>* out_listeners);

#endif  // CHROME_TEST_CHROMEDRIVER_LOGGING_H_
