// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/common/scheduler_switches.h"

namespace scheduler {
namespace switches {

// Enable Virtualized time where the render thread's time source skips forward
// to the next scheduled delayed time if there is no more non-delayed work to be
// done.
const char kEnableVirtualizedTime[] = "enable-virtualized-time";

// Disable task throttling of timer tasks from background pages.
const char kDisableBackgroundTimerThrottling[] =
    "disable-background-timer-throttling";

}  // namespace switches
}  // namespace scheduler
