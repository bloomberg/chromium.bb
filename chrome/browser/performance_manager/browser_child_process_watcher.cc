// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/browser_child_process_watcher.h"

#include <memory>
#include <utility>

#include "base/process/process.h"
#include "base/stl_util.h"
#include "chrome/browser/performance_manager/performance_manager.h"
#include "chrome/browser/performance_manager/process_resource_coordinator.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/common/process_type.h"

namespace performance_manager {

BrowserChildProcessWatcher::BrowserChildProcessWatcher()
    : browser_node_(PerformanceManager::GetInstance()) {
  browser_node_.OnProcessLaunched(base::Process::Current());
  BrowserChildProcessObserver::Add(this);
}

BrowserChildProcessWatcher::~BrowserChildProcessWatcher() {
  BrowserChildProcessObserver::Remove(this);
}

void BrowserChildProcessWatcher::BrowserChildProcessLaunchedAndConnected(
    const content::ChildProcessData& data) {
  if (data.process_type == content::PROCESS_TYPE_GPU) {
    std::unique_ptr<performance_manager::ProcessResourceCoordinator> gpu_node =
        std::make_unique<ProcessResourceCoordinator>(
            PerformanceManager::GetInstance());
    gpu_node->OnProcessLaunched(data.GetProcess());
    gpu_process_nodes_[data.id] = std::move(gpu_node);
  }
}

void BrowserChildProcessWatcher::BrowserChildProcessHostDisconnected(
    const content::ChildProcessData& data) {
  if (data.process_type == content::PROCESS_TYPE_GPU) {
    size_t removed = gpu_process_nodes_.erase(data.id);
    DCHECK_EQ(1u, removed);
  }
}

void BrowserChildProcessWatcher::BrowserChildProcessCrashed(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& info) {
  if (data.process_type == content::PROCESS_TYPE_GPU)
    GPUProcessExited(data.id, info.exit_code);
}

void BrowserChildProcessWatcher::BrowserChildProcessKilled(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& info) {
  if (data.process_type == content::PROCESS_TYPE_GPU)
    GPUProcessExited(data.id, info.exit_code);
}

void BrowserChildProcessWatcher::GPUProcessExited(int id, int exit_code) {
  // It appears the exit code can be delivered either after the host is
  // disconnected, or perhaps before the HostConnected notification,
  // specifically on crash.
  if (base::ContainsKey(gpu_process_nodes_, id)) {
    auto* process_node = gpu_process_nodes_[id].get();
    process_node->SetProcessExitStatus(exit_code);
  }
}

}  // namespace performance_manager
