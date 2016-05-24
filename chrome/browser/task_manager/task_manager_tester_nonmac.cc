// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/task_manager_tester.h"

#include "chrome/browser/task_management/task_manager_interface.h"

// TODO(tapted): Delete this file and rename CreateDefault() to Create() when
// there is no longer a legacy task manager.

namespace task_management {

std::unique_ptr<TaskManagerTester> TaskManagerTester::Create(
    const base::Closure& callback) {
#if defined(OS_MACOSX)
  // This Create() method doesn't attempt to create a tester for the legacy
  // interface, so disabling the new task manager on Mac have no effect.
  DCHECK(TaskManagerInterface::IsNewTaskManagerEnabled());
#endif
  return TaskManagerTester::CreateDefault(callback);
}

void TaskManagerTester::MaybeRefreshLegacyInstance() {
#if defined(OS_MACOSX)
  DCHECK(TaskManagerInterface::IsNewTaskManagerEnabled());
#endif
}

}  // namespace task_management
