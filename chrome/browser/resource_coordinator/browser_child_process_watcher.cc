// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/browser_child_process_watcher.h"

#include <memory>

#include "base/process/process.h"
#include "chrome/browser/performance_manager/performance_manager.h"
#include "chrome/browser/performance_manager/process_resource_coordinator.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/common/process_type.h"

namespace resource_coordinator {

BrowserChildProcessWatcher::BrowserChildProcessWatcher() {
  BrowserChildProcessObserver::Add(this);
}

BrowserChildProcessWatcher::~BrowserChildProcessWatcher() {
  BrowserChildProcessObserver::Remove(this);
}

void BrowserChildProcessWatcher::BrowserChildProcessLaunchedAndConnected(
    const content::ChildProcessData& data) {
  if (data.process_type == content::PROCESS_TYPE_GPU) {
    gpu_process_resource_coordinator_ =
        std::make_unique<performance_manager::ProcessResourceCoordinator>(
            performance_manager::PerformanceManager::GetInstance());
    gpu_process_resource_coordinator_->OnProcessLaunched(data.GetProcess());
  }
}

void BrowserChildProcessWatcher::BrowserChildProcessHostDisconnected(
    const content::ChildProcessData& data) {
  if (data.process_type == content::PROCESS_TYPE_GPU)
    GPUProcessStopped();
}

void BrowserChildProcessWatcher::BrowserChildProcessCrashed(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& info) {
  if (data.process_type == content::PROCESS_TYPE_GPU)
    GPUProcessStopped();
}

void BrowserChildProcessWatcher::BrowserChildProcessKilled(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& info) {
  if (data.process_type == content::PROCESS_TYPE_GPU)
    GPUProcessStopped();
}

void BrowserChildProcessWatcher::GPUProcessStopped() {
  gpu_process_resource_coordinator_.reset();
}

}  // namespace resource_coordinator
