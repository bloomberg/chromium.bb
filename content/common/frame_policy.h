// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FRAME_POLICY_H_
#define CONTENT_COMMON_FRAME_POLICY_H_

#include "content/common/content_export.h"
#include "content/common/feature_policy/feature_policy.h"
#include "third_party/WebKit/public/web/WebSandboxFlags.h"

namespace content {

// This structure contains the attributes of a frame which determine what
// features are available during the lifetime of the framed document. Currently,
// this includes the sandbox flags and the feature policy container policy. Used
// in the frame tree to track sandbox and feature policy in the browser process,
// and tranferred over IPC during frame replication when site isolation is
// enabled.
//
// Unlike the attributes in FrameOwnerProperties, these attributes are never
// updated after the framed document has been loaded, so two versions of this
// structure are kept in the frame tree for each frame -- the effective policy
// and the pending policy, which will take effect when the frame is next
// navigated.
struct CONTENT_EXPORT FramePolicy {
  FramePolicy();
  FramePolicy(blink::WebSandboxFlags sandbox_flags,
              const ParsedFeaturePolicyHeader& container_policy);
  FramePolicy(const FramePolicy& lhs);
  ~FramePolicy();

  blink::WebSandboxFlags sandbox_flags;
  ParsedFeaturePolicyHeader container_policy;
};

}  // namespace content

#endif  // CONTENT_COMMON_FRAME_POLICY_H_
