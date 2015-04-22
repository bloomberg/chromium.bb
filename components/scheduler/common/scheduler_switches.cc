// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/common/scheduler_switches.h"

namespace scheduler {
namespace switches {

// Disable the Blink Scheduler. Ensures there's no reordering of blink tasks.
// This switch is intended only for performance tests.
const char kDisableBlinkScheduler[] = "disable-blink-scheduler";

}  // namespace switches
}  // namespace scheduler
