// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/capabilities.h"

namespace gpu {

Capabilities::PerStagePrecisions::PerStagePrecisions() {
}

Capabilities::Capabilities() {}

Capabilities::Capabilities(const Capabilities& other) = default;

}  // namespace gpu
