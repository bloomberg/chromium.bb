// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_CHILD_PROCESS_TASK_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_CHILD_PROCESS_TASK_PROVIDER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/task_management/providers/task_provider.h"
#include "content/public/browser/browser_child_process_observer.h"

namespace content {
struct ChildProcessData;
}

namespace task_management {

class ChildProcessTask;

// Defines a provider to provide the tasks that represent various types of child
// processes such as the GPU process or a plugin process ... etc.
class ChildProcessTaskProvider
    : public TaskProvider,
      public content::BrowserChildProcessObserver {
 public:
  ChildProcessTaskProvider();
  ~ChildProcessTaskProvider() override;

  // task_management::TaskProvider:
  Task* GetTaskOfUrlRequest(int origin_pid,
                            int child_id,
                            int route_id) override;

  // content::BrowserChildProcessObserver:
  void BrowserChildProcessLaunchedAndConnected(
      const content::ChildProcessData& data) override;
  void BrowserChildProcessHostDisconnected(
      const content::ChildProcessData& data) override;

 private:
  friend class ChildProcessTaskTest;

  // task_management::TaskProvider:
  void StartUpdating() override;
  void StopUpdating() override;

  // The pre-existing child processes data will be collected on the IO thread.
  // When that is done, we will be notified on the UI thread by receiving a call
  // to this method.
  void ChildProcessDataCollected(
      std::unique_ptr<const std::vector<content::ChildProcessData>>
          child_processes);

  // Creates a ChildProcessTask from the given |data| and notifies the observer
  // of its addition.
  void CreateTask(const content::ChildProcessData& data);

  // Deletes a ChildProcessTask whose |handle| is provided after notifying the
  // observer of its deletion.
  void DeleteTask(base::ProcessHandle handle);

  // A map to track ChildProcessTask's by their handles.
  using HandleToTaskMap = std::map<base::ProcessHandle, ChildProcessTask*>;
  HandleToTaskMap tasks_by_handle_;

  // A map to track ChildProcessTask's by their PIDs.
  //
  // Why have both |tasks_by_handle_| and |tasks_by_pid_|? On Windows, where
  // handles are not the same as PIDs, |DeleteTask| gets only a handle, which
  // may be closed, making it impossible to query the PID from the handle. So
  // we need an index on the handle. Meanwhile, we also need an index on the
  // PID so that we can efficiently implement |GetTaskOfUrlRequest()|, which
  // gets only a PID.
  //
  // TODO(afakhry): Fix this either by keeping the handle open via
  // |base::Process|, or amending the |BrowserChildProcessObserver| interface to
  // supply the PID.
  using PidToTaskMap = std::map<base::ProcessId, ChildProcessTask*>;
  PidToTaskMap tasks_by_pid_;

  // Always keep this the last member of this class to make sure it's the
  // first thing to be destructed.
  base::WeakPtrFactory<ChildProcessTaskProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcessTaskProvider);
};

}  // namespace task_management

#endif  // CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_CHILD_PROCESS_TASK_PROVIDER_H_
