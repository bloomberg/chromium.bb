// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_SWITCHES_INTERNAL_H_
#define CONTENT_COMMON_CONTENT_SWITCHES_INTERNAL_H_

#include "content/public/common/web_preferences.h"

namespace content {

bool IsPinchToZoomEnabled();
#if defined(OS_WIN)
// Disables Win32k Renderer lockdown for any future renderer child processes.
void DisableWin32kRendererLockdown();

// Returns whether Win32k Renderer lockdown is enabled or not.
bool IsWin32kRendererLockdownEnabled();
// Returns whether Win32k PPAPI lockdown is enabled for a specific mime type.
bool IsWin32kLockdownEnabledForMimeType(const std::string& mime_type);
#endif
V8CacheOptions GetV8CacheOptions();

bool IsUseZoomForDSFEnabled();

} // namespace content

#endif  // CONTENT_COMMON_CONTENT_SWITCHES_INTERNAL_H_
