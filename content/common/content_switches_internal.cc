// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_switches_internal.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"

namespace content {

bool IsOverlayScrollbarEnabled() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kDisableOverlayScrollbar))
    return false;
  else if (command_line.HasSwitch(switches::kEnableOverlayScrollbar))
    return true;

  return false;
}

} // namespace content
