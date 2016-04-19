// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_ARC_ARC_PROCESS_TASK_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_ARC_ARC_PROCESS_TASK_PROVIDER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "chrome/browser/chromeos/arc/arc_process.h"
#include "chrome/browser/task_management/providers/arc/arc_process_task.h"
#include "chrome/browser/task_management/providers/task_provider.h"

namespace task_management {

// This provides the ARC process tasks.
//
// Since this provider obtains ARC process information via IPC and procfs,
// it can never avoid race conditions. For example, in an extreme case such as
// fork(2) is called millions of times in a second, this provider can return
// wrong results. However, its chance is very low, and even if we hit the case,
// the worst outcome is just that an app (non-system) process which
// the user did not intend to choose is killed. Since apps are designed
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
  // Auto-retry if ARC bridge service is not ready.
  void RequestProcessList();

  void OnUpdateProcessList(const std::vector<arc::ArcProcess>& processes);

  // task_management::TaskProvider:
  void StartUpdating() override;
  void StopUpdating() override;

  void ScheduleNextRequest();

  std::map<base::ProcessId, std::unique_ptr<ArcProcessTask>> nspid_to_task_;

  // Whether to continue the periodical polling.
  bool is_updating_;

  // Always keep this the last member of this class to make sure it's the
  // first thing to be destructed.
  base::WeakPtrFactory<ArcProcessTaskProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcProcessTaskProvider);
};

}  // namespace task_management

#endif  // CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_ARC_ARC_PROCESS_TASK_PROVIDER_H_
