// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_CHILD_PROCESS_TASK_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_CHILD_PROCESS_TASK_H_

#include "chrome/browser/task_management/providers/task.h"

namespace content {
struct ChildProcessData;
}  // namespace content

namespace task_management {

// Represents several types of the browser's child processes such as
// a plugin or a GPU process, ... etc.
class ChildProcessTask : public Task {
 public:
  // Creates a child process task given its |data| which is
  // received from observing |content::BrowserChildProcessObserver|.
  explicit ChildProcessTask(const content::ChildProcessData& data);

  ~ChildProcessTask() override;

  // task_management::Task:
  Type GetType() const override;
  int GetChildProcessUniqueID() const override;

 private:
  // The unique ID of the child process. It is not the PID of the process.
  // See |content::ChildProcessData::id|.
  const int unique_child_process_id_;

  // The type of the child process. See |content::ProcessType| and
  // |NaClTrustedProcessType|.
  const int process_type_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcessTask);
};

}  // namespace task_management


#endif  // CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_CHILD_PROCESS_TASK_H_
