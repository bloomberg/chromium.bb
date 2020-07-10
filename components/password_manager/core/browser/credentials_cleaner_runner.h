// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIALS_CLEANER_RUNNER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIALS_CLEANER_RUNNER_H_

#include <memory>

#include "base/containers/queue.h"
#include "base/macros.h"
#include "components/password_manager/core/browser/credentials_cleaner.h"

namespace password_manager {

// This class is responsible of running credential clean-ups sequentially in the
// order they are added. The runner is informed by the the clean-up tasks that
// the clean-up is finished when a clean-up task calls CleaningCompleted. This
// class will keep the clean-up object alive until the runner is notified that
// the clean-up is finished.
// Usage:
// (1) Add cleaning tasks in the order the have to be executed.
// (2) After all cleaning task are added call StartCleaning().
// Important note: The object will delete itself once all clean-ups are done,
// thus it should not be allocated on the stack. Having a non-public destructor
// enforces this.
class CredentialsCleanerRunner : public CredentialsCleaner::Observer {
 public:
  CredentialsCleanerRunner();
  ~CredentialsCleanerRunner() override;

  // Adds |cleaning_task| to the |cleaning_task_queue_| if the corresponding
  // cleaning task still needs to be done.
  void MaybeAddCleaningTask(std::unique_ptr<CredentialsCleaner> cleaning_task);

  bool HasPendingTasks() const;

  void StartCleaning();

  // CredentialsCleaner::Observer:
  void CleaningCompleted() override;

 private:
  void StartCleaningTask();

  bool cleaning_started_ = false;

  base::queue<std::unique_ptr<CredentialsCleaner>> cleaning_tasks_queue_;

  DISALLOW_COPY_AND_ASSIGN(CredentialsCleanerRunner);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIALS_CLEANER_RUNNER_H_
