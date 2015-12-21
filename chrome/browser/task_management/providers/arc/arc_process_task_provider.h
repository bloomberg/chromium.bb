// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_ARC_ARC_PROCESS_TASK_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_ARC_ARC_PROCESS_TASK_PROVIDER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/task_management/providers/arc/arc_process_task.h"
#include "chrome/browser/task_management/providers/task_provider.h"
#include "components/arc/arc_bridge_service.h"

namespace base {
class Timer;
}  // namespace base

namespace task_management {

// This provides the ARC process tasks.
//
// Since this provider obtains ARC process information via IPC and procfs,
// it can never avoid race conditions. For example, in an extreme case such as
// fork(2) is called millions of times in a second, this provider can return
// wrong results. However, its chance is very low, and even if we hit the case,
// the worst outcome is just that an Android app (non-system) process which
// the user did not intend to choose is killed. Since Android apps are designed
// to be killed at any time, it sounds acceptable.
class ArcProcessTaskProvider : public TaskProvider {
 public:
  ArcProcessTaskProvider();
  ~ArcProcessTaskProvider() override;

  // task_management::TaskProvider:
  Task* GetTaskOfUrlRequest(int origin_pid,
                            int child_id,
                            int route_id) override;

 private:
  void RequestProcessList();
  void OnUpdateProcessList(
      mojo::Array<arc::RunningAppProcessInfoPtr> processes);

  void RemoveTasks(const std::set<base::ProcessId>& removed_nspids);
  void AddTasks(const std::vector<arc::RunningAppProcessInfo>& added_processes,
                const std::map<base::ProcessId, base::ProcessId>& nspid_to_pid);

  // task_management::TaskProvider:
  void StartUpdating() override;
  void StopUpdating() override;

  std::map<base::ProcessId, scoped_ptr<ArcProcessTask> > nspid_to_task_;
  base::RepeatingTimer timer_;

  // Always keep this the last member of this class to make sure it's the
  // first thing to be destructed.
  base::WeakPtrFactory<ArcProcessTaskProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcProcessTaskProvider);
};

}  // namespace task_management

#endif  // CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_ARC_ARC_PROCESS_TASK_PROVIDER_H_
