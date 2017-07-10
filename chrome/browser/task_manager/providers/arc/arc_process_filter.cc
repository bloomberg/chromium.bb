// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/arc/arc_process_filter.h"

#include <utility>

#include "base/logging.h"
#include "components/arc/common/process.mojom.h"

namespace task_manager {

namespace {

const char* kDefaultWhitelist[] = {"/system/bin/surfaceflinger"};

}  // namespace

ArcProcessFilter::ArcProcessFilter()
    : whitelist_(std::begin(kDefaultWhitelist), std::end(kDefaultWhitelist)) {}

ArcProcessFilter::ArcProcessFilter(const std::set<std::string>& whitelist)
    : whitelist_(whitelist) {}

ArcProcessFilter::~ArcProcessFilter() = default;

bool ArcProcessFilter::ShouldDisplayProcess(
    const arc::ArcProcess& process) const {
  // Check the explicit whitelist. If it is not in there, decide based on its
  // ProcessState.
  if (whitelist_.count(process.process_name()))
    return true;
  // Implicitly whitelist processes that might be important to the user.
  // See process.mojom.
  // ProcessState::TOP: Process is hosting the current top activities
  // ProcessState::IMPORTANT_FOREGROUND: Process is important to the user
  // ProcessState::TOP_SLEEPING: Same as TOP, but while the device is sleeping
  // ProcessState::LAST_ACTIVITY: The last activity - see crbug.com/738651.
  // ProcessState::FOREGROUND_SERVICE: Foreground service - in Android, the user
  //   is made aware of these via a notification.
  switch (process.process_state()) {
    case arc::mojom::ProcessState::TOP:  // Fallthrough.
    case arc::mojom::ProcessState::IMPORTANT_FOREGROUND:
    case arc::mojom::ProcessState::TOP_SLEEPING:
    case arc::mojom::ProcessState::LAST_ACTIVITY:
    case arc::mojom::ProcessState::FOREGROUND_SERVICE:
      return true;
    case arc::mojom::ProcessState::NONEXISTENT:  // Fallthrough.
    case arc::mojom::ProcessState::PERSISTENT:
    case arc::mojom::ProcessState::PERSISTENT_UI:
    case arc::mojom::ProcessState::BOUND_FOREGROUND_SERVICE:
    case arc::mojom::ProcessState::IMPORTANT_BACKGROUND:
    case arc::mojom::ProcessState::BACKUP:
    case arc::mojom::ProcessState::HEAVY_WEIGHT:
    case arc::mojom::ProcessState::SERVICE:
    case arc::mojom::ProcessState::RECEIVER:
    case arc::mojom::ProcessState::HOME:
    case arc::mojom::ProcessState::CACHED_ACTIVITY:
    case arc::mojom::ProcessState::CACHED_ACTIVITY_CLIENT:
    case arc::mojom::ProcessState::CACHED_EMPTY:
      return false;
  }
  NOTREACHED();
  return false;
}

}  // namespace task_manager
