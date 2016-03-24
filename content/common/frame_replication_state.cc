// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/frame_replication_state.h"
#include "third_party/WebKit/public/web/WebSandboxFlags.h"
#include "third_party/WebKit/public/web/WebTreeScopeType.h"

namespace content {

FrameReplicationState::FrameReplicationState()
    : sandbox_flags(blink::WebSandboxFlags::None),
      scope(blink::WebTreeScopeType::Document),
      should_enforce_strict_mixed_content_checking(false),
      has_potentially_trustworthy_unique_origin(false) {}

FrameReplicationState::FrameReplicationState(
    blink::WebTreeScopeType scope,
    const std::string& name,
    const std::string& unique_name,
    blink::WebSandboxFlags sandbox_flags,
    bool should_enforce_strict_mixed_content_checking,
    bool has_potentially_trustworthy_unique_origin)
    : origin(),
      sandbox_flags(sandbox_flags),
      name(name),
      unique_name(unique_name),
      scope(scope),
      should_enforce_strict_mixed_content_checking(
          should_enforce_strict_mixed_content_checking),
      has_potentially_trustworthy_unique_origin(
          has_potentially_trustworthy_unique_origin) {}

FrameReplicationState::FrameReplicationState(
    const FrameReplicationState& other) = default;

FrameReplicationState::~FrameReplicationState() {
}

}  // namespace content
