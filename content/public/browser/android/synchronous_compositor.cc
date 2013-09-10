// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/android/synchronous_compositor.h"

namespace content {

SynchronousCompositorMemoryPolicy::SynchronousCompositorMemoryPolicy()
    : bytes_limit(0), num_resources_limit(0) {}

}  // namespace content
