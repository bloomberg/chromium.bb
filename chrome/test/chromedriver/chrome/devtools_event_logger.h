// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_EVENT_LOGGER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_EVENT_LOGGER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/status.h"

// Accumulates DevTools events of a given type for use in the Logging API.
// Tracks all WebViews of a given Chrome, via their respective DevToolsClients.
//
// A log message has the following JSON structure:
// {
//    "level": logging_level,
//    "timestamp": <milliseconds since epoch>,
//    "message": <JSON string described below>
// }
// The message attribute is a JSON string of the following structure:
// {
//    "webview": <originating WebView ID>,
//    "message": { "method": "...", "params": { ... }}  // DevTools message.
// }
class DevToolsEventLogger : public DevToolsEventListener {
 public:
  explicit DevToolsEventLogger(const std::string& log_type,
                               const std::vector<std::string>& domains,
                               const std::string& logging_level);
  virtual ~DevToolsEventLogger();

  const std::string& GetLogType();
  scoped_ptr<base::ListValue> GetAndClearLogEntries();

  virtual Status OnConnected(DevToolsClient* client) OVERRIDE;
  virtual void OnEvent(DevToolsClient* client,
                       const std::string& method,
                       const base::DictionaryValue& params) OVERRIDE;

 private:
  virtual bool ShouldLogEvent(const std::string& method);
  virtual scoped_ptr<DictionaryValue> GetLogEntry(
      DevToolsClient* client,
      const std::string& method,
      const base::DictionaryValue& params);

  const std::string log_type_;
  const std::vector<std::string> domains_;
  const std::string logging_level_;
  scoped_ptr<ListValue> log_entries_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsEventLogger);
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_EVENT_LOGGER_H_
