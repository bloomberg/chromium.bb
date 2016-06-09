// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/providers/arc/arc_process_task_provider.h"

#include <stddef.h>

#include <queue>
#include <set>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/process/process.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/chromeos/arc/arc_process.h"
#include "chrome/browser/chromeos/arc/arc_process_service.h"
#include "components/arc/common/process.mojom.h"

namespace task_management {

namespace {

const int kUpdateProcessListDelaySeconds = 1;

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

void ArcProcessTaskProvider::OnUpdateProcessList(
    const std::vector<ArcProcess>& processes) {
  TRACE_EVENT0("browser", "ArcProcessTaskProvider::OnUpdateProcessList");

  if (!is_updating_)
    return;

  // NB: |processes| can be already stale here because it is sent via IPC, and
  // we can never avoid that. See also the comment at the declaration of
  // ArcProcessTaskProvider.

  set<ProcessId> nspid_to_remove;
  for (const auto& entry : nspid_to_task_)
    nspid_to_remove.insert(entry.first);

  for (const auto& entry : processes) {
    if (nspid_to_remove.erase(entry.nspid()) == 0) {
      // New arc process.
      std::unique_ptr<ArcProcessTask>& task = nspid_to_task_[entry.nspid()];
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
      std::unique_ptr<ArcProcessTask>& task = nspid_to_task_[entry.nspid()];
      DCHECK(task.get());
      task->SetProcessState(entry.process_state());
    }
  }

  for (const auto& entry : nspid_to_remove) {
    // Stale arc process.
    NotifyObserverTaskRemoved(nspid_to_task_[entry].get());
    nspid_to_task_.erase(entry);
  }
  ScheduleNextRequest();
}

void ArcProcessTaskProvider::RequestProcessList() {
  arc::ArcProcessService* arc_process_service =
      arc::ArcProcessService::Get();
  auto callback = base::Bind(&ArcProcessTaskProvider::OnUpdateProcessList,
                             weak_ptr_factory_.GetWeakPtr());
  if (!arc_process_service ||
      !arc_process_service->RequestProcessList(callback)) {
    VLOG(2) << "ARC process instance is not ready.";
    // Update with the empty ARC process list.
    // Note that this can happen in the middle of the session if the user has
    // just opted out from ARC.
    callback.Run(std::vector<ArcProcess>());
  }
}

void ArcProcessTaskProvider::StartUpdating() {
  is_updating_ = true;
  RequestProcessList();
}

void ArcProcessTaskProvider::StopUpdating() {
  is_updating_ = false;
  nspid_to_task_.clear();
}

void ArcProcessTaskProvider::ScheduleNextRequest() {
  if (!is_updating_)
    return;
  // TODO(nya): Remove this timer once ARC starts to send us UpdateProcessList
  // message when the process list changed. As of today, ARC does not send
  // the process list unless we request it by RequestProcessList message.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ArcProcessTaskProvider::RequestProcessList,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kUpdateProcessListDelaySeconds));
}

}  // namespace task_management
