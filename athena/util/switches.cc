// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/util/switches.h"

#include "base/command_line.h"

namespace athena {
namespace switches {

bool IsDebugAcceleratorsEnabled() {
#if NDEBUG
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      "debug-accelerators");
#else
  return true;
#endif
}

}  // namespace switches
}  // namespace athena
