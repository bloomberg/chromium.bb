// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_LOGS_SINGLE_LOG_SOURCE_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_LOGS_SINGLE_LOG_SOURCE_H_

#include <stddef.h>

#include "base/files/file.h"
#include "base/macros.h"
#include "chrome/browser/feedback/system_logs/system_logs_fetcher_base.h"
#include "components/feedback/anonymizer_tool.h"

namespace system_logs {

// Gathers log data from a single source, possibly incrementally.
class SingleLogSource : public SystemLogsSource {
 public:
  enum class SupportedSource {
    // For /var/log/messages.
    kMessages,

    // For /var/log/ui/ui.LATEST.
    kUiLatest,
  };

  explicit SingleLogSource(SupportedSource source);
  ~SingleLogSource() override;

  // system_logs::SystemLogsSource:
  void Fetch(const SysLogsSourceCallback& callback) override;

 private:
  friend class SingleLogSourceTest;

  // Reads all available content from |file_| that has not already been read.
  void ReadFile(SystemLogsResponse* result);

  // Keeps track of how much data has been read from |file_|.
  size_t num_bytes_read_;

  // Handle for reading the log file that is source of logging data.
  base::File file_;

  // For removing PII from log results.
  feedback::AnonymizerTool anonymizer_;

  base::WeakPtrFactory<SingleLogSource> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SingleLogSource);
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_LOGS_SINGLE_LOG_SOURCE_H_
