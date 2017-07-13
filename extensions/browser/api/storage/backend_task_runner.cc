// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/storage/backend_task_runner.h"

#include "base/task_scheduler/lazy_task_runner.h"

namespace extensions {

base::LazySequencedTaskRunner g_sequenced_task_task_runner =
    LAZY_SEQUENCED_TASK_RUNNER_INITIALIZER({base::MayBlock()});

scoped_refptr<base::SequencedTaskRunner> GetBackendTaskRunner() {
  return g_sequenced_task_task_runner.Get();
}

bool IsOnBackendSequence() {
  return GetBackendTaskRunner()->RunsTasksInCurrentSequence();
}

}  // namespace extensions
