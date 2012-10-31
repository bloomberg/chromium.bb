// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_COMPOSITOR_UTIL_H_
#define CONTENT_PUBLIC_BROWSER_COMPOSITOR_UTIL_H_

#include "content/common/content_export.h"

namespace content {

// Returns true if the threaded compositor is on (via flags or field trial).
CONTENT_EXPORT bool IsThreadedCompositingEnabled();

// Returns true if force-compositing-mode is on (via flags or field trial).
CONTENT_EXPORT bool IsForceCompositingModeEnabled();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_COMPOSITOR_UTIL_H_
