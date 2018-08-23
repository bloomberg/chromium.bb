// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_SCANNER_SCANNER_CONTROLLER_H_
#define CHROME_CHROME_CLEANER_SCANNER_SCANNER_CONTROLLER_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "chrome/chrome_cleaner/constants/uws_id.h"
#include "chrome/chrome_cleaner/logging/registry_logger.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"

namespace chrome_cleaner {

// An abstract class which handles synchronization for the scan loop.
class ScannerController {
 public:
  virtual ~ScannerController();

  int ScanOnly();

 protected:
  explicit ScannerController(RegistryLogger* registry_logger);

  virtual void StartScan() = 0;

  // Callback for StartScan which records which PUPs were found.
  // Expects |status| to be RESULT_CODE_SUCCESS if there were no failures.
  virtual void DoneScanning(ResultCode status,
                            const std::vector<UwSId>& found_pups);

  // Record |found_pups| to the registry and update |result_code_| in a
  // thread-safe manner.
  void UpdateScanResults(const std::vector<UwSId>& found_pups);

  // This callback is called from the watchdog's thread and must synchronize
  // access to result_code_.
  virtual int WatchdogTimeoutCallback();

  RegistryLogger* registry_logger_;
  SEQUENCE_CHECKER(sequence_checker_);
  // Defines a task runner for the current thread, which will be accessible
  // via base::ThreadTaskRunnerHandle::Get().
  base::MessageLoopForUI ui_message_loop_;

  // Allow subclasses to override the default watchdog timeout.
  uint32_t watchdog_timeout_in_seconds_;

 private:
  // Callback for LoggingServiceAPI::SendLogsToSafeBrowsing() that finishes the
  // current run loop.
  void LogsUploadComplete(bool success);

  mutable base::Lock lock_;  // Protects |result_code_|.
  ResultCode result_code_ = RESULT_CODE_INVALID;

  // Called by LogsUploadComplete() to quit the current run loop.
  base::OnceClosure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(ScannerController);
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_SCANNER_SCANNER_CONTROLLER_H_
