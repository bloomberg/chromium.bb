// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_task.h"

#include "base/bind.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_manager.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_token.h"
#include "chrome/browser/sync_file_system/drive_backend/task_dependency_manager.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

void CallRunExclusive(const base::WeakPtr<ExclusiveTask>& task,
                      scoped_ptr<SyncTaskToken> token) {
  if (task)
    task->RunExclusive(SyncTaskToken::WrapToCallback(token.Pass()));
}

}  // namespace

ExclusiveTask::ExclusiveTask() : weak_ptr_factory_(this) {}
ExclusiveTask::~ExclusiveTask() {}

void ExclusiveTask::RunPreflight(scoped_ptr<SyncTaskToken> token) {
  scoped_ptr<BlockingFactor> blocking_factor(new BlockingFactor);
  blocking_factor->exclusive = true;

  SyncTaskManager::UpdateBlockingFactor(
      token.Pass(), blocking_factor.Pass(),
      base::Bind(&CallRunExclusive, weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace drive_backend
}  // namespace sync_file_system
