// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_switches_internal.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "content/public/common/content_switches.h"

#if defined(OS_WIN)
#include "base/strings/string_tokenizer.h"
#include "base/win/windows_version.h"
#include "ui/gfx/win/direct_write.h"
#endif

namespace content {

namespace {

#if defined(OS_WIN)
static bool g_win32k_renderer_lockdown_disabled = false;
#endif

}  // namespace

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

bool IsWin32kLockdownEnabledForMimeType(const std::string& mime_type) {
  // Consider PPAPI lockdown a superset of renderer lockdown.
  if (!IsWin32kRendererLockdownEnabled())
    return false;
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();

  std::string mime_types =
      base::FieldTrialList::FindFullName("EnableWin32kLockDownMimeTypes");
  if (cmd_line->HasSwitch(switches::kEnableWin32kLockDownMimeTypes)) {
    mime_types =
        cmd_line->GetSwitchValueASCII(switches::kEnableWin32kLockDownMimeTypes);
  }

  // Consider the value * to enable all mime types for lockdown.
  if (mime_types == "*")
    return true;

  base::StringTokenizer tokenizer(mime_types, ",");
  tokenizer.set_quote_chars("\"");
  while (tokenizer.GetNext()) {
    if (tokenizer.token() == mime_type)
      return true;
  }

  return false;
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
  static bool enabled = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableUseZoomForDSF);
  return enabled;
}

} // namespace content
