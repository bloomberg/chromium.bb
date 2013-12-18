// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_CHROME_INTERNAL_LOG_SOURCE_H_
#define CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_CHROME_INTERNAL_LOG_SOURCE_H_

#include "chrome/browser/feedback/system_logs/system_logs_fetcher_base.h"

namespace system_logs {

// Fetches internal Chrome logs.
class ChromeInternalLogSource : public SystemLogsSource {
 public:
  ChromeInternalLogSource() {}
  virtual ~ChromeInternalLogSource() {}

  // SystemLogsSource override.
  virtual void Fetch(const SysLogsSourceCallback& request) OVERRIDE;

 private:
  void PopulateSyncLogs(SystemLogsResponse* response);
  void PopulateExtensionInfoLogs(SystemLogsResponse* response);

  DISALLOW_COPY_AND_ASSIGN(ChromeInternalLogSource);
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_CHROME_INTERNAL_LOG_SOURCE_H_
