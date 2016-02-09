// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_switches_internal.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "ui/gfx/win/direct_write.h"
#endif

#if defined(OS_CHROMEOS)
#include "base/sys_info.h"
#endif

namespace content {

namespace {

#if defined(OS_WIN)
static bool g_win32k_renderer_lockdown_disabled = false;
#endif

bool IsUseZoomForDSFEnabledByDefault() {
#if defined(OS_CHROMEOS)
  // TODO(oshima): Device emulation needs to be fixed to pass
  // all tests on bots. crbug.com/584709.
  return base::SysInfo::IsRunningOnChromeOS();
#else
  return false;
#endif
}

}  // namespace

bool IsPinchToZoomEnabled() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // Enable pinch everywhere unless it's been explicitly disabled.
  return !command_line.HasSwitch(switches::kDisablePinch);
}

#if defined(OS_WIN)

void DisableWin32kRendererLockdown() {
  g_win32k_renderer_lockdown_disabled = true;
}

bool IsWin32kRendererLockdownEnabled() {
  if (g_win32k_renderer_lockdown_disabled)
    return false;
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return false;
  if (!gfx::win::ShouldUseDirectWrite())
    return false;
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kDisableWin32kRendererLockDown))
    return false;
  return true;
}

#endif

V8CacheOptions GetV8CacheOptions() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string v8_cache_options =
      command_line.GetSwitchValueASCII(switches::kV8CacheOptions);
  if (v8_cache_options.empty())
    v8_cache_options = base::FieldTrialList::FindFullName("V8CacheOptions");
  if (v8_cache_options == "none") {
    return V8_CACHE_OPTIONS_NONE;
  } else if (v8_cache_options == "parse") {
    return V8_CACHE_OPTIONS_PARSE;
  } else if (v8_cache_options == "code") {
    return V8_CACHE_OPTIONS_CODE;
  } else {
    return V8_CACHE_OPTIONS_DEFAULT;
  }
}

bool IsUseZoomForDSFEnabled() {
  static bool use_zoom_for_dsf_enabled_by_default =
      IsUseZoomForDSFEnabledByDefault();
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool enabled =
      (command_line->HasSwitch(switches::kEnableUseZoomForDSF) ||
       use_zoom_for_dsf_enabled_by_default) &&
      command_line->GetSwitchValueASCII(
          switches::kEnableUseZoomForDSF) != "false";

  return enabled;
}

} // namespace content
