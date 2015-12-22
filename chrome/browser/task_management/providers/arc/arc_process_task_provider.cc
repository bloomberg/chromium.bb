// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/providers/arc/arc_process_task_provider.h"

#include <queue>
#include <set>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/process/process.h"
#include "base/process/process_iterator.h"
#include "base/task_runner_util.h"
#include "base/threading/worker_pool.h"
#include "base/trace_event/trace_event.h"
#include "components/arc/arc_bridge_service.h"

namespace {

// Computes a map from PID in ARC namespace to PID in system namespace.
// The returned map contains ARC processes only.
std::map<base::ProcessId, base::ProcessId> ComputeNspidToPidMap() {
  TRACE_EVENT0("browser", "ComputeNspidToPidMap");

  // NB: Despite of its name, ProcessIterator::Snapshot() may return
  // inconsistent information because it simply walks procfs. Especially
  // we must not assume the parent-child relationships are consistent.
  const base::ProcessIterator::ProcessEntries& entry_list =
      base::ProcessIterator(nullptr).Snapshot();

  // System may contain many different namespaces so several different
  // processes may have the same nspid. We need to get the proper subset of
  // processes to create correct nspid -> pid map.

  // Construct the process tree.
  // NB: This can contain a loop in case of race conditions.
  std::map<base::ProcessId, std::vector<base::ProcessId> > process_tree;
  for (const base::ProcessEntry& entry : entry_list)
    process_tree[entry.parent_pid()].push_back(entry.pid());

  // Find the ARC init process.
  base::ProcessId arc_init_pid = base::kNullProcessId;
  for (const base::ProcessEntry& entry : entry_list) {
    // TODO(nya): Add more constraints to avoid mismatches.
    std::string process_name =
        !entry.cmd_line_args().empty() ? entry.cmd_line_args()[0] : "";
    if (process_name == "/init") {
      arc_init_pid = entry.pid();
      break;
    }
  }

  // Enumerate all processes under ARC init and create nspid -> pid map.
  std::map<base::ProcessId, base::ProcessId> nspid_to_pid;
  if (arc_init_pid != base::kNullProcessId) {
    std::queue<base::ProcessId> queue;
    std::set<base::ProcessId> visited;
    queue.push(arc_init_pid);
    while (!queue.empty()) {
      base::ProcessId pid = queue.front();
      queue.pop();
      // Do not visit the same process twice. Otherwise we may enter an infinite
      // loop if |process_tree| contains a loop.
      if (!visited.insert(pid).second)
        continue;
      base::ProcessId nspid = base::Process(pid).GetPidInNamespace();
      // All ARC processes should be in namespace so nspid is usually non-null,
      // but this can happen if the process has already gone.
      if (nspid != base::kNullProcessId)
        nspid_to_pid[nspid] = pid;
      for (base::ProcessId child_pid : process_tree[pid])
        queue.push(child_pid);
    }
  }
  return nspid_to_pid;
}

}  // namespace

namespace task_management {

ArcProcessTaskProvider::ArcProcessTaskProvider() : weak_ptr_factory_(this) {}

ArcProcessTaskProvider::~ArcProcessTaskProvider() {}

Task* ArcProcessTaskProvider::GetTaskOfUrlRequest(int origin_pid,
                                                  int child_id,
                                                  int route_id) {
  // ARC tasks are not associated with any URL request.
  return nullptr;
}

void ArcProcessTaskProvider::OnUpdateProcessList(
    mojo::Array<arc::RunningAppProcessInfoPtr> processes) {
  TRACE_EVENT0("browser", "ArcProcessTaskProvider::OnUpdateProcessList");

  // NB: |processes| can be already stale here because it is sent via IPC, and
  // we can never avoid that. See also the comment at the declaration of
  // ArcProcessTaskProvider.

  std::vector<arc::RunningAppProcessInfo> added_processes;
  std::set<base::ProcessId> removed_nspids;
  for (const auto& entry : nspid_to_task_)
    removed_nspids.insert(entry.first);
  for (size_t i = 0; i < processes.size(); ++i) {
    const arc::RunningAppProcessInfoPtr& process = processes[i];
    if (nspid_to_task_.count(process->pid))
      removed_nspids.erase(process->pid);
    else
      added_processes.push_back(*process);
  }

  // Remove stale tasks.
  RemoveTasks(removed_nspids);

  // Add new tasks.
  // Note: ComputeNspidToPidMap() is an expensive operation, so try to reduce
  // the number of times it is called.
  if (!added_processes.empty()) {
    base::PostTaskAndReplyWithResult(
        base::WorkerPool::GetTaskRunner(false /* task_is_slow */).get(),
        FROM_HERE,
        base::Bind(&ComputeNspidToPidMap),
        base::Bind(&ArcProcessTaskProvider::AddTasks,
                   weak_ptr_factory_.GetWeakPtr(),
                   added_processes));
  }
}

void ArcProcessTaskProvider::RequestProcessList() {
  arc::ProcessInstance* arc_process_instance =
      arc::ArcBridgeService::Get()->process_instance();
  if (!arc_process_instance) {
    VLOG(2) << "ARC process instance is not ready.";
    return;
  }
  arc_process_instance->RequestProcessList(
      base::Bind(&ArcProcessTaskProvider::OnUpdateProcessList,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ArcProcessTaskProvider::RemoveTasks(
    const std::set<base::ProcessId>& removed_nspids) {
  for (base::ProcessId nspid : removed_nspids) {
    NotifyObserverTaskRemoved(nspid_to_task_[nspid].get());
    nspid_to_task_.erase(nspid);
  }
}

void ArcProcessTaskProvider::AddTasks(
    const std::vector<arc::RunningAppProcessInfo>& added_processes,
    const std::map<base::ProcessId, base::ProcessId>& nspid_to_pid) {
  for (const arc::RunningAppProcessInfo& process : added_processes) {
    auto iter = nspid_to_pid.find(process.pid);
    if (iter == nspid_to_pid.end()) {
      // The process may have exited just after the snapshot was generated.
      continue;
    }
    base::ProcessId pid = iter->second;
    scoped_ptr<ArcProcessTask>& task = nspid_to_task_[process.pid];
    task.reset(new ArcProcessTask(pid, process.pid, process.process_name));
    NotifyObserverTaskAdded(task.get());
  }
}

void ArcProcessTaskProvider::StartUpdating() {
  RequestProcessList();
  // TODO(nya): Remove this timer once ARC starts to send us UpdateProcessList
  // message when the process list changed. As of today, ARC does not send
  // the process list unless we request it by RequestProcessList message.
  timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(1),
      this,
      &ArcProcessTaskProvider::RequestProcessList);
}

void ArcProcessTaskProvider::StopUpdating() {
  timer_.Stop();
  nspid_to_task_.clear();
}

}  // namespace task_management
