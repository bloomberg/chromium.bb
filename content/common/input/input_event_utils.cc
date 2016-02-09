// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/input_event_utils.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"

namespace content {

bool UseGestureBasedWheelScrolling() {
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  return cmd->HasSwitch(switches::kEnableWheelGestures);
}

}  // namespace content
