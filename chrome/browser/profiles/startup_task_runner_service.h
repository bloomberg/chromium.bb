// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_STARTUP_TASK_RUNNER_SERVICE_H_
#define CHROME_BROWSER_PROFILES_STARTUP_TASK_RUNNER_SERVICE_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace base {
class DeferredSequencedTaskRunner;
}  // namespace base

// This service manages the startup task runners.
class StartupTaskRunnerService : public base::NonThreadSafe,
                                 public KeyedService {
 public:
  explicit StartupTaskRunnerService(Profile* profile);
  virtual ~StartupTaskRunnerService();

  // Returns sequenced task runner where all bookmarks I/O operations are
  // performed.
  // This method should only be called from the UI thread.
  // Note: Using a separate task runner per profile service gives a better
  // management of the sequence in which the task are started in order to avoid
  // congestion during start-up (e.g the caller may decide to start loading the
  // bookmarks only after the history finished).
  scoped_refptr<base::DeferredSequencedTaskRunner> GetBookmarkTaskRunner();

  // Starts the task runners that are deferred during start-up.
  void StartDeferredTaskRunners();

 private:
  Profile* profile_;
  scoped_refptr<base::DeferredSequencedTaskRunner> bookmark_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(StartupTaskRunnerService);
};

#endif  // CHROME_BROWSER_PROFILES_STARTUP_TASK_RUNNER_SERVICE_H_
