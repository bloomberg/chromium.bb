// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FRAME_REPLICATION_STATE_H_
#define CONTENT_COMMON_FRAME_REPLICATION_STATE_H_

#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

// This structure holds information that needs to be replicated from a
// RenderFrame to any of its associated RenderFrameProxies.
struct CONTENT_EXPORT FrameReplicationState {
  FrameReplicationState();
  ~FrameReplicationState();

  // Current serialized security origin of the frame. Unique origins are
  // represented as the string "null" per RFC 6454.
  url::Origin origin;

  // TODO(alexmos): Eventually, this structure can also hold other state that
  // needs to be replicated, such as frame sizing info.
};

}  // namespace content

#endif // CONTENT_COMMON_FRAME_REPLICATION_STATE_H_
