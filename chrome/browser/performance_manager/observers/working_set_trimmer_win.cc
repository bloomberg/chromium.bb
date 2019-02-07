// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/observers/working_set_trimmer_win.h"

#include <windows.h>  // Must be in front of other Windows header files.

#include <psapi.h>

#include "base/logging.h"
#include "base/process/process.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/graph/node_base.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"

namespace performance_manager {

namespace {

// Empties the working set of a process with id |process_id| and creation time
// |process_creation_time|. The creation time is verified to ensure that we
// don't empty the working set of the wrong process if the target process exits
// and its id is reused.
void EmptyWorkingSet(base::ProcessId process_id,
                     base::Time process_creation_time) {
  base::Process process = base::Process::OpenWithAccess(
      process_id, PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_SET_QUOTA);
  if (!process.IsValid()) {
    DPLOG(ERROR) << "Working set not emptied because process handle could not "
                    "be obtained.";
    return;
  }

  if (process.CreationTime() != process_creation_time) {
    DLOG(ERROR) << "Working set not emptied because actual process creation "
                   "time does not match expected process creation time";
    return;
  }

#if DCHECK_IS_ON()
  BOOL empty_working_set_success =
#endif
      ::EmptyWorkingSet(process.Handle());
  DPLOG_IF(ERROR, !empty_working_set_success)
      << "Working set not emptied because EmptyWorkingSet() failed";
}

}  // namespace

WorkingSetTrimmer::WorkingSetTrimmer() = default;
WorkingSetTrimmer::~WorkingSetTrimmer() = default;

bool WorkingSetTrimmer::ShouldObserve(const NodeBase* coordination_unit) {
  return coordination_unit->id().type ==
         resource_coordinator::CoordinationUnitType::kProcess;
}

void WorkingSetTrimmer::OnAllFramesInProcessFrozen(
    const ProcessNodeImpl* process_cu) {
  const base::ProcessId process_id = process_cu->process_id();
  if (process_id != base::kNullProcessId) {
    EmptyWorkingSet(process_id, process_cu->launch_time());
  }
}

}  // namespace performance_manager
