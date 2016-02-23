// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/input_event_utils.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"

namespace content {

bool UseGestureBasedWheelScrolling() {
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
// TODO(dtapuska): OSX has special code to deal with mouse wheel
// and overscroll that needs to be fixed. crbug.com/587979
#if defined(OS_MACOSX)
  return cmd->HasSwitch(switches::kEnableWheelGestures);
#else
  return !cmd->HasSwitch(switches::kDisableWheelGestures);
#endif
}

}  // namespace content
