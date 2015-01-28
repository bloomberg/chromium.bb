// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_switches_internal.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "content/public/common/content_switches.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "ui/gfx/win/direct_write.h"
#endif

namespace content {

bool IsPinchToZoomEnabled() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // --disable-pinch should always disable pinch
  if (command_line.HasSwitch(switches::kDisablePinch))
    return false;

#if defined(OS_WIN)
  return base::win::GetVersion() >= base::win::VERSION_WIN8;
#elif defined(OS_CHROMEOS)
  return true;
#else
  return command_line.HasSwitch(switches::kEnableViewport) ||
      command_line.HasSwitch(switches::kEnablePinch);
#endif
}

#if defined(OS_WIN)

bool IsWin32kRendererLockdownEnabled() {
  const std::string group_name =
      base::FieldTrialList::FindFullName("Win32kLockdown");
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return false;
  if (!gfx::win::ShouldUseDirectWrite())
    return false;
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kEnableWin32kRendererLockDown))
    return true;
  if (cmd_line->HasSwitch(switches::kDisableWin32kRendererLockDown))
    return false;
  // Default.
  return group_name == "Enabled";
}
#endif

} // namespace content
