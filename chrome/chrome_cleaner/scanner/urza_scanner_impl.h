// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_SCANNER_URZA_SCANNER_IMPL_H_
#define CHROME_CHROME_CLEANER_SCANNER_URZA_SCANNER_IMPL_H_

#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/sequence_checker.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"
#include "chrome/chrome_cleaner/scanner/scanner.h"
#include "chrome/chrome_cleaner/settings/matching_options.h"

namespace chrome_cleaner {

// Forward declarations.
class SignatureMatcherAPI;
class RegistryLogger;

// This class is used to scan for the footprints of all PUPs in PUPData. It uses
// a worker thread pool to scan asynchronously and be interruptable, but is not
// thread safe, in the sense that the Start/Stop/IsStopDone methods must all be
// called from the same thread that created the class instance.
class UrzaScannerImpl : public Scanner {
 public:
  // Create instance of scanner that uses Urza engine.
  // If |registry_logger| is not null, write scan times of individual PUPs to
  // the registry. Any |RegistryLogger| object pointed to by |registry_logger|
  // must stay alive for the entire lifetime of this scanner instance.
  UrzaScannerImpl(const MatchingOptions& options,
                  SignatureMatcherAPI* signature_matcher,
                  RegistryLogger* registry_logger);

  ~UrzaScannerImpl() override;

  // UrzaScanner.

  // Start a set of tasks in worker pool threads to scan the disk and registry
  // for PUP footprints. A PUP is deemed "found" if at least one of its
  // footprints can be found. If a PUP was found, |found_uws_callback| is
  // called. If the scan completes before |Stop| is called, then |done_callback|
  // is called. Returns true if scan startup succeeds.
  bool Start(const FoundUwSCallback& found_uws_callback,
             DoneCallback done_callback) override;
  void Stop() override;

  // When calling |Stop|, some tasks may still be running, so make sure to call
  // |IsCompletelyDone| and allow the main UI to pump messages to let the task
  // tracker mark all tasks as done before clearing PUPData, which should not be
  // an issue in production code where it's all static, but can be an issue with
  // dynamic PUPData in test code. Return true if there's nothing left to be
  // completed asynchronously by the scanner (i.e, a stop was started and
  // finished, or everything was completely done).
  bool IsCompletelyDone() const override;

 private:
  void UwSFound(UwSId found_uws_id, base::ScopedClosureRunner task_done);

  void InvokeAllTasksDone(scoped_refptr<base::TaskRunner> task_runner);

  void AllTasksDone();

  MatchingOptions options_;
  SignatureMatcherAPI* signature_matcher_;
  RegistryLogger* registry_logger_;

  // The task tracker that can be used to cancel pending tasks.
  base::CancelableTaskTracker cancelable_task_tracker_;
  SEQUENCE_CHECKER(sequence_checker_);

  // Callbacks to report progress and completion.
  FoundUwSCallback found_uws_callback_;
  DoneCallback done_callback_;

  // To accumulate the list of found PUPs passed to |TaskCompleted|.
  std::vector<UwSId> found_pups_;

  // Whether all tasks are done, either having been cancelled or completed.
  bool all_tasks_done_;

  // The number of PUPs, which is also the total number of tasks to run.
  size_t num_pups_;

  // Whether |Stop| has been called or not. Reset to false in |Start|.
  bool stopped_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_SCANNER_URZA_SCANNER_IMPL_H_
