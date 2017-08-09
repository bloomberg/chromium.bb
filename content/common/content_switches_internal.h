// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_SWITCHES_INTERNAL_H_
#define CONTENT_COMMON_CONTENT_SWITCHES_INTERNAL_H_

#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/web_preferences.h"

namespace content {

bool IsPinchToZoomEnabled();
#if defined(OS_WIN)
// Returns whether Win32k lockdown is enabled for child processes or not.
bool IsWin32kLockdownEnabled();
#endif
V8CacheOptions GetV8CacheOptions();

ProgressBarCompletion GetProgressBarCompletionPolicy();

CONTENT_EXPORT bool IsUseZoomForDSFEnabled();

void WaitForDebugger(const std::string& label);

} // namespace content

#endif  // CONTENT_COMMON_CONTENT_SWITCHES_INTERNAL_H_
