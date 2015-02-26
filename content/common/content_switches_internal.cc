// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_switches_internal.h"

#include <string>

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
  return group_name != "Disabled";
}
#endif

V8CacheOptions GetV8CacheOptions() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string v8_cache_options =
      command_line.GetSwitchValueASCII(switches::kV8CacheOptions);
  if (v8_cache_options.empty())
    v8_cache_options = base::FieldTrialList::FindFullName("V8CacheOptions");
  if (v8_cache_options == "parse") {
    return V8_CACHE_OPTIONS_PARSE;
  } else if (v8_cache_options == "code") {
    return V8_CACHE_OPTIONS_CODE;
  } else if (v8_cache_options == "code-compressed") {
    return V8_CACHE_OPTIONS_CODE_COMPRESSED;
  } else if (v8_cache_options == "none") {
    return V8_CACHE_OPTIONS_NONE;
  } else if (v8_cache_options == "parse-memory") {
    return V8_CACHE_OPTIONS_PARSE_MEMORY;
  } else if (v8_cache_options == "heuristics") {
    return V8_CACHE_OPTIONS_HEURISTICS;
  } else if (v8_cache_options == "heuristics-mobile") {
    return V8_CACHE_OPTIONS_HEURISTICS_MOBILE;
  } else if (v8_cache_options == "heuristics-default") {
    return V8_CACHE_OPTIONS_HEURISTICS_DEFAULT;
  } else if (v8_cache_options == "heuristics-default-mobile") {
    return V8_CACHE_OPTIONS_HEURISTICS_DEFAULT_MOBILE;
  } else if (v8_cache_options == "recent") {
    return V8_CACHE_OPTIONS_RECENT;
  } else if (v8_cache_options == "recent-small") {
    return V8_CACHE_OPTIONS_RECENT_SMALL;
  } else {
    return V8_CACHE_OPTIONS_DEFAULT;
  }
}

} // namespace content
