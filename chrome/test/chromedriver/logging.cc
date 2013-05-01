// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/logging.h"

#include "chrome/test/chromedriver/capabilities.h"
#include "chrome/test/chromedriver/chrome/devtools_event_logger.h"
#include "chrome/test/chromedriver/chrome/status.h"

Status CreateLoggers(const Capabilities& capabilities,
                     ScopedVector<DevToolsEventLogger>* out_loggers) {
  if (capabilities.logging_prefs) {
    ScopedVector<DevToolsEventLogger> loggers;
    for (DictionaryValue::Iterator pref(*capabilities.logging_prefs);
         !pref.IsAtEnd(); pref.Advance()) {
      const std::string type = pref.key();
      std::string level;
      if (!pref.value().GetAsString(&level)) {
        return Status(kUnknownError,
                      "logging level must be a string for log type: " + type);
      }
      if ("performance" == type) {
        std::vector<std::string> domains;
        domains.push_back("Network");
        domains.push_back("Page");
        domains.push_back("Timeline");
        loggers.push_back(new DevToolsEventLogger(type, domains, level));
      } else {
        return Status(kUnknownError, "unsupported log type: " + type);
      }
      // TODO(klm): Implement and add here the console logger.
    }
    out_loggers->swap(loggers);
  }
  return Status(kOk);
}
