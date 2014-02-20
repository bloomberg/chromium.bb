// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_COMPOSITOR_UTIL_H_
#define CONTENT_BROWSER_GPU_COMPOSITOR_UTIL_H_

#include "base/values.h"
#include "content/common/content_export.h"

namespace content {

// Returns true if the threaded compositor is on (via flags or field trial).
CONTENT_EXPORT bool IsThreadedCompositingEnabled();

// Returns true if force-compositing-mode is on (via flags or field trial).
CONTENT_EXPORT bool IsForceCompositingModeEnabled();

// Returns true if delegated-renderer is on (via flags, or platform default).
CONTENT_EXPORT bool IsDelegatedRendererEnabled();

CONTENT_EXPORT base::Value* GetFeatureStatus();
CONTENT_EXPORT base::Value* GetProblems();
CONTENT_EXPORT base::Value* GetDriverBugWorkarounds();

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_COMPOSITOR_UTIL_H_
