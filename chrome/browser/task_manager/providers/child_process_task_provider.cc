// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/child_process_task_provider.h"

#include "base/process/process.h"
#include "chrome/browser/task_manager/providers/child_process_task.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"

using content::BrowserChildProcessHostIterator;
using content::BrowserThread;
using content::ChildProcessData;

namespace task_manager {

namespace {

// Collects and returns the child processes data on the IO thread to get all the
// pre-existing child process before we start observing
// |BrowserChildProcessObserver|.
std::unique_ptr<std::vector<ChildProcessData>> CollectChildProcessData() {
  // The |BrowserChildProcessHostIterator| must only be used on the IO thread.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::unique_ptr<std::vector<ChildProcessData>> child_processes(
      new std::vector<ChildProcessData>());
  for (BrowserChildProcessHostIterator itr; !itr.Done(); ++itr) {
    const ChildProcessData& process_data = itr.GetData();

    // Only add processes that have already started, i.e. with valid handles.
    if (process_data.handle == base::kNullProcessHandle)
      continue;

    child_processes->push_back(process_data);
  }

  return child_processes;
}

}  // namespace

ChildProcessTaskProvider::ChildProcessTaskProvider()
    : weak_ptr_factory_(this) {
}

ChildProcessTaskProvider::~ChildProcessTaskProvider() {
}

Task* ChildProcessTaskProvider::GetTaskOfUrlRequest(int child_id,
                                                    int route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto itr = tasks_by_child_id_.find(child_id);
  if (itr == tasks_by_child_id_.end())
    return nullptr;

  return itr->second;
}

void ChildProcessTaskProvider::BrowserChildProcessLaunchedAndConnected(
    const content::ChildProcessData& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (data.handle == base::kNullProcessHandle)
    return;

  CreateTask(data);
}

void ChildProcessTaskProvider::BrowserChildProcessHostDisconnected(
    const content::ChildProcessData& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DeleteTask(data.handle);
}

void ChildProcessTaskProvider::StartUpdating() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(tasks_by_handle_.empty());
  DCHECK(tasks_by_child_id_.empty());

  // First, get the pre-existing child processes data.
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&CollectChildProcessData),
      base::Bind(&ChildProcessTaskProvider::ChildProcessDataCollected,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ChildProcessTaskProvider::StopUpdating() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // ChildProcessDataCollected() should never be called after this, and hence
  // we must invalidate the weak pointers.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // First, stop observing.
  BrowserChildProcessObserver::Remove(this);

  // Remember: You can't notify the observer of tasks removal here,
  // StopUpdating() is called after the observer has been cleared.

  // Then delete all tasks (if any).
  tasks_by_handle_.clear();
  tasks_by_child_id_.clear();
}

void ChildProcessTaskProvider::ChildProcessDataCollected(
    std::unique_ptr<const std::vector<content::ChildProcessData>>
        child_processes) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (const auto& process_data : *child_processes)
    CreateTask(process_data);

  // Now start observing.
  BrowserChildProcessObserver::Add(this);
}

void ChildProcessTaskProvider::CreateTask(
    const content::ChildProcessData& data) {
  std::unique_ptr<ChildProcessTask>& task = tasks_by_handle_[data.handle];
  if (task) {
    // This task is already known to us. This case can happen when some of the
    // child process data we collect upon StartUpdating() might be of
    // BrowserChildProcessHosts whose process hadn't launched yet. So we just
    // return.
    return;
  }

  // Create the task and notify the observer.
  task.reset(new ChildProcessTask(data));
  tasks_by_child_id_[task->GetChildProcessUniqueID()] = task.get();
  NotifyObserverTaskAdded(task.get());
}

void ChildProcessTaskProvider::DeleteTask(base::ProcessHandle handle) {
  auto itr = tasks_by_handle_.find(handle);

  // The following case should never happen since we start observing
  // |BrowserChildProcessObserver| only after we collect all pre-existing child
  // processes and are notified (on the UI thread) that the collection is
  // completed at |ChildProcessDataCollected()|.
  if (itr == tasks_by_handle_.end()) {
    // BUG(crbug.com/611067): Temporarily removing due to test flakes. The
    // reason why this happens is well understood (see bug), but there's no
    // quick and easy fix.
    // NOTREACHED();
    return;
  }

  NotifyObserverTaskRemoved(itr->second.get());

  // Clear from the child_id index.
  tasks_by_child_id_.erase(itr->second->GetChildProcessUniqueID());

  // Finally delete the task.
  tasks_by_handle_.erase(itr);
}

}  // namespace task_manager
