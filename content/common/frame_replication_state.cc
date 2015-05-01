// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/frame_replication_state.h"

namespace content {

FrameReplicationState::FrameReplicationState()
    : FrameReplicationState("", SandboxFlags::NONE) {
}

FrameReplicationState::FrameReplicationState(const std::string& name,
                                             SandboxFlags sandbox_flags)
    : origin(), sandbox_flags(sandbox_flags), name(name) {
}

FrameReplicationState::~FrameReplicationState() {
}

}  // namespace content
