// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The main point of this class is to cache ARC proc nspid<->pid mapping
// globally. Since the calculation is costly, a dedicated worker thread is
// used. All read/write of its internal data structure (i.e., the mapping)
// should be on this thread.

#include "chrome/browser/chromeos/arc/arc_process_service.h"

#include <queue>
#include <set>
#include <string>

#include "base/process/process.h"
#include "base/process/process_iterator.h"
#include "base/task_runner_util.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_thread.h"

namespace arc {

namespace {

const char kSequenceToken[] = "arc_process_service";

// Weak pointer.  This class is owned by ArcServiceManager.
ArcProcessService* g_arc_process_service = nullptr;

}  // namespace

using base::kNullProcessId;
using base::Process;
using base::ProcessId;
using base::SequencedWorkerPool;
using std::map;
using std::set;
using std::vector;

ArcProcessService::ArcProcessService(ArcBridgeService* bridge_service)
    : ArcService(bridge_service),
      worker_pool_(new SequencedWorkerPool(1, "arc_process_manager")),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc_bridge_service()->AddObserver(this);
  DCHECK(!g_arc_process_service);
  g_arc_process_service = this;
  // Not intended to be used from the creating thread.
  thread_checker_.DetachFromThread();
}

ArcProcessService::~ArcProcessService() {
  DCHECK(g_arc_process_service == this);
  g_arc_process_service = nullptr;
  arc_bridge_service()->RemoveObserver(this);
  worker_pool_->Shutdown();
}

// static
ArcProcessService* ArcProcessService::Get() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return g_arc_process_service;
}

void ArcProcessService::OnProcessInstanceReady() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  worker_pool_->PostNamedSequencedWorkerTask(
      kSequenceToken,
      FROM_HERE,
      base::Bind(&ArcProcessService::Reset,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ArcProcessService::Reset() {
  DCHECK(thread_checker_.CalledOnValidThread());
  nspid_to_pid_.clear();
}

bool ArcProcessService::RequestProcessList(
    RequestProcessListCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  arc::mojom::ProcessInstance* process_instance =
      arc_bridge_service()->process_instance();
  if (!process_instance) {
    return false;
  }
  process_instance->RequestProcessList(
      base::Bind(&ArcProcessService::OnReceiveProcessList,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
  return true;
}

void ArcProcessService::OnReceiveProcessList(
    const RequestProcessListCallback& callback,
    mojo::Array<arc::mojom::RunningAppProcessInfoPtr> mojo_processes) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto raw_processes = new vector<mojom::RunningAppProcessInfoPtr>();
  mojo_processes.Swap(raw_processes);

  auto ret_processes = new vector<ArcProcess>();
  // Post to its dedicated worker thread to avoid race condition.
  // Since no two tasks with the same token should be run at the same.
  // Note: GetSequencedTaskRunner's shutdown behavior defaults to
  // SKIP_ON_SHUTDOWN (ongoing task blocks shutdown).
  // So in theory using Unretained(this) should be fine since the life cycle
  // of |this| is the same as the main browser.
  // To be safe I still use weak pointers, but weak_ptrs can only bind to
  // methods without return values. That's why I can't use
  // PostTaskAndReplyWithResult but handle the return object by myself.
  auto runner = worker_pool_->GetSequencedTaskRunner(
      worker_pool_->GetNamedSequenceToken(kSequenceToken));
  runner->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&ArcProcessService::UpdateAndReturnProcessList,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Owned(raw_processes),
                 base::Unretained(ret_processes)),
      base::Bind(&ArcProcessService::CallbackRelay,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 base::Owned(ret_processes)));
}

void ArcProcessService::CallbackRelay(
    const RequestProcessListCallback& callback,
    const vector<ArcProcess>* ret_processes) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  callback.Run(*ret_processes);
}

void ArcProcessService::UpdateAndReturnProcessList(
    const vector<arc::mojom::RunningAppProcessInfoPtr>* raw_processes,
    vector<ArcProcess>* ret_processes) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Cleanup dead pids in the cache |nspid_to_pid_|.
  set<ProcessId> nspid_to_remove;
  for (const auto& entry : nspid_to_pid_) {
    nspid_to_remove.insert(entry.first);
  }
  bool unmapped_nspid = false;
  for (const auto& entry : *raw_processes) {
    // erase() returns 0 if coudln't find the key. It means a new process.
    if (nspid_to_remove.erase(entry->pid) == 0) {
      nspid_to_pid_[entry->pid] = kNullProcessId;
      unmapped_nspid = true;
    }
  }
  for (const auto& entry : nspid_to_remove) {
    nspid_to_pid_.erase(entry);
  }

  // The operation is costly so avoid calling it when possible.
  if (unmapped_nspid) {
    UpdateNspidToPidMap();
  }

  PopulateProcessList(raw_processes, ret_processes);
}

void ArcProcessService::PopulateProcessList(
    const vector<arc::mojom::RunningAppProcessInfoPtr>* raw_processes,
    vector<ArcProcess>* ret_processes) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (const auto& entry : *raw_processes) {
    const auto it = nspid_to_pid_.find(entry->pid);
    // In case the process already dies so couldn't find corresponding pid.
    if (it != nspid_to_pid_.end() && it->second != kNullProcessId) {
      ArcProcess arc_process = {
          entry->pid, it->second, entry->process_name, entry->process_state};
      ret_processes->push_back(arc_process);
    }
  }
}

// Computes a map from PID in ARC namespace to PID in system namespace.
// The returned map contains ARC processes only.
void ArcProcessService::UpdateNspidToPidMap() {
  DCHECK(thread_checker_.CalledOnValidThread());

  TRACE_EVENT0("browser", "ArcProcessService::UpdateNspidToPidMap");

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
  map<ProcessId, vector<ProcessId> > process_tree;
  for (const base::ProcessEntry& entry : entry_list)
    process_tree[entry.parent_pid()].push_back(entry.pid());

  // Find the ARC init process.
  ProcessId arc_init_pid = kNullProcessId;
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
  if (arc_init_pid != kNullProcessId) {
    std::queue<ProcessId> queue;
    std::set<ProcessId> visited;
    queue.push(arc_init_pid);
    while (!queue.empty()) {
      ProcessId pid = queue.front();
      queue.pop();
      // Do not visit the same process twice. Otherwise we may enter an infinite
      // loop if |process_tree| contains a loop.
      if (!visited.insert(pid).second)
        continue;

      ProcessId nspid = base::Process(pid).GetPidInNamespace();

      // All ARC processes should be in namespace so nspid is usually non-null,
      // but this can happen if the process has already gone.
      // Only add processes we're interested in (those appear as keys in
      // |nspid_to_pid_|).
      if (nspid != kNullProcessId &&
          nspid_to_pid_.find(nspid) != nspid_to_pid_.end())
        nspid_to_pid_[nspid] = pid;

      for (ProcessId child_pid : process_tree[pid])
        queue.push(child_pid);
    }
  }
}

}  // namespace arc
