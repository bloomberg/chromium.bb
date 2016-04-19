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
#include "base/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/chromeos/arc/arc_process.h"
#include "chrome/browser/chromeos/arc/arc_process_service.h"

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
  for (const auto& it : nspid_to_task_)
    nspid_to_remove.insert(it.first);

  for (const auto& it : processes) {
    if (nspid_to_remove.erase(it.nspid) == 0) {
      // New arc process.
      std::unique_ptr<ArcProcessTask>& task = nspid_to_task_[it.nspid];
      // After calling NotifyObserverTaskAdded(), the raw pointer of |task| is
      // remebered somewhere else. One should not (implicitly) delete the
      // referenced object before calling NotifyObserverTaskRemoved() first
      // (crbug.com/587707).
      DCHECK(!task.get()) <<
          "Task with the same pid should not be added twice.";
      task.reset(new ArcProcessTask(it.pid, it.nspid, it.process_name));
      NotifyObserverTaskAdded(task.get());
    }
  }

  for (const auto& it : nspid_to_remove) {
    // Stale arc process.
    NotifyObserverTaskRemoved(nspid_to_task_[it].get());
    nspid_to_task_.erase(it);
  }
  ScheduleNextRequest();
}

void ArcProcessTaskProvider::RequestProcessList() {
  arc::ArcProcessService* arc_process_service =
      arc::ArcProcessService::Get();
  if (!arc_process_service ||
      !arc_process_service->RequestProcessList(
          base::Bind(&ArcProcessTaskProvider::OnUpdateProcessList,
                     weak_ptr_factory_.GetWeakPtr()))) {
    VLOG(2) << "ARC process instance is not ready.";
    ScheduleNextRequest();
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
