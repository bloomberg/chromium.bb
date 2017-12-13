// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_MEMORY_DETAILS_LOG_SOURCE_H_
#define CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_MEMORY_DETAILS_LOG_SOURCE_H_

#include "base/macros.h"
#include "components/feedback/system_logs/system_logs_source.h"

namespace system_logs {

// Fetches memory usage details.
class MemoryDetailsLogSource : public SystemLogsSource {
 public:
  MemoryDetailsLogSource();
  ~MemoryDetailsLogSource() override;

  // SystemLogsSource override.
  void Fetch(SysLogsSourceCallback request) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MemoryDetailsLogSource);
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_MEMORY_DETAILS_LOG_SOURCE_H_
