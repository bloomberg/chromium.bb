// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/arc/arc_process_task_provider.h"

#include <stddef.h>

#include <queue>
#include <set>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/chromeos/arc/process/arc_process_service.h"
#include "components/arc/common/process.mojom.h"

namespace task_manager {

namespace {

const int kUpdateAppProcessListDelaySeconds = 1;
const int kUpdateSystemProcessListDelaySeconds = 3;

}  // namespace

using std::set;
using arc::ArcProcess;
using base::Process;
using base::ProcessId;

ArcProcessTaskProvider::ArcProcessTaskProvider()
    : is_updating_(false), weak_ptr_factory_(this) {}

ArcProcessTaskProvider::~ArcProcessTaskProvider() {}

Task* ArcProcessTaskProvider::GetTaskOfUrlRequest(int origin_pid,
                                                  int child_id,
                                                  int route_id) {
  // ARC tasks are not associated with any URL request.
  return nullptr;
}

void ArcProcessTaskProvider::UpdateProcessList(
    ArcTaskMap* pid_to_task,
    const std::vector<ArcProcess>& processes) {
  if (!is_updating_)
    return;

  // NB: |processes| can be already stale here because it is sent via IPC, and
  // we can never avoid that. See also the comment at the declaration of
  // ArcProcessTaskProvider.

  set<ProcessId> nspid_to_remove;
  for (const auto& entry : *pid_to_task)
    nspid_to_remove.insert(entry.first);

  for (const auto& entry : processes) {
    // Skip adding or updating processes we will not display.
    if (!process_filter_.ShouldDisplayProcess(entry))
      continue;

    if (nspid_to_remove.erase(entry.nspid()) == 0) {
      // New arc process.
      std::unique_ptr<ArcProcessTask>& task = (*pid_to_task)[entry.nspid()];
      // After calling NotifyObserverTaskAdded(), the raw pointer of |task| is
      // remebered somewhere else. One should not (implicitly) delete the
      // referenced object before calling NotifyObserverTaskRemoved() first
      // (crbug.com/587707).
      DCHECK(!task.get()) <<
          "Task with the same pid should not be added twice.";
      task.reset(new ArcProcessTask(entry.pid(), entry.nspid(),
                                    entry.process_name(), entry.process_state(),
                                    entry.packages()));
      NotifyObserverTaskAdded(task.get());
    } else {
      // Update process state of existing process.
      std::unique_ptr<ArcProcessTask>& task = (*pid_to_task)[entry.nspid()];
      DCHECK(task.get());
      task->SetProcessState(entry.process_state());
    }
  }

  for (const auto& entry : nspid_to_remove) {
    // Stale arc process.
    NotifyObserverTaskRemoved((*pid_to_task)[entry].get());
    pid_to_task->erase(entry);
  }
}

void ArcProcessTaskProvider::OnUpdateAppProcessList(
    std::vector<ArcProcess> processes) {
  TRACE_EVENT0("browser", "ArcProcessTaskProvider::OnUpdateAppProcessList");
  UpdateProcessList(&nspid_to_task_, processes);
  ScheduleNextAppRequest();
}

void ArcProcessTaskProvider::OnUpdateSystemProcessList(
    std::vector<ArcProcess> processes) {
  UpdateProcessList(&nspid_to_sys_task_, processes);
  ScheduleNextSystemRequest();
}

void ArcProcessTaskProvider::RequestAppProcessList() {
  arc::ArcProcessService* arc_process_service =
      arc::ArcProcessService::Get();
  auto callback = base::Bind(&ArcProcessTaskProvider::OnUpdateAppProcessList,
                             weak_ptr_factory_.GetWeakPtr());
  if (!arc_process_service ||
      !arc_process_service->RequestAppProcessList(callback)) {
    VLOG(2) << "ARC process instance is not ready.";
    ScheduleNextAppRequest();
    return;
  }
}

void ArcProcessTaskProvider::RequestSystemProcessList() {
  arc::ArcProcessService* arc_process_service = arc::ArcProcessService::Get();
  auto callback = base::Bind(&ArcProcessTaskProvider::OnUpdateSystemProcessList,
                             weak_ptr_factory_.GetWeakPtr());
  if (!arc_process_service) {
    VLOG(2) << "ARC process instance is not ready.";
    ScheduleNextSystemRequest();
    return;
  }
  arc_process_service->RequestSystemProcessList(callback);
}

void ArcProcessTaskProvider::StartUpdating() {
  is_updating_ = true;
  RequestAppProcessList();
  RequestSystemProcessList();
}

void ArcProcessTaskProvider::StopUpdating() {
  is_updating_ = false;
  nspid_to_task_.clear();
  nspid_to_sys_task_.clear();
}

void ArcProcessTaskProvider::ScheduleNextRequest(const base::Closure& task,
                                                 const int delaySeconds) {
  if (!is_updating_)
    return;
  // TODO(nya): Remove this timer once ARC starts to send us UpdateProcessList
  // message when the process list changed. As of today, ARC does not send
  // the process list unless we request it by RequestAppProcessList message.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, task, base::TimeDelta::FromSeconds(delaySeconds));
}

void ArcProcessTaskProvider::ScheduleNextAppRequest() {
  ScheduleNextRequest(base::Bind(&ArcProcessTaskProvider::RequestAppProcessList,
                                 weak_ptr_factory_.GetWeakPtr()),
                      kUpdateAppProcessListDelaySeconds);
}

void ArcProcessTaskProvider::ScheduleNextSystemRequest() {
  ScheduleNextRequest(
      base::Bind(&ArcProcessTaskProvider::RequestSystemProcessList,
                 weak_ptr_factory_.GetWeakPtr()),
      kUpdateSystemProcessListDelaySeconds);
}

}  // namespace task_manager
