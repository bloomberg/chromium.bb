// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_service_manager.h"

#include "base/sequenced_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "components/arc/arc_bridge_service_impl.h"

namespace arc {

namespace {

// Weak pointer.  This class is owned by ChromeBrowserMainPartsChromeos.
ArcServiceManager* g_arc_service_manager = nullptr;

}  // namespace

ArcServiceManager::ArcServiceManager(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& file_task_runner)
    : origin_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      arc_bridge_service_(
          new ArcBridgeServiceImpl(io_task_runner, file_task_runner)) {
  DCHECK(!g_arc_service_manager);
  g_arc_service_manager = this;
}

ArcServiceManager::~ArcServiceManager() {
  DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(g_arc_service_manager == this);
  g_arc_service_manager = nullptr;
}

// static
ArcServiceManager* ArcServiceManager::Get() {
  DCHECK(
      g_arc_service_manager->origin_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(g_arc_service_manager);
  return g_arc_service_manager;
}

ArcBridgeService* ArcServiceManager::arc_bridge_service() {
  DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());
  return arc_bridge_service_.get();
}

}  // namespace arc
