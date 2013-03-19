// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_LOGS_CHROME_INTERNAL_LOG_SOURCE_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_LOGS_CHROME_INTERNAL_LOG_SOURCE_H_

#include "chrome/browser/chromeos/system_logs/system_logs_fetcher.h"

namespace chromeos {

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

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_LOGS_CHROME_INTERNAL_LOG_SOURCE_H_
