// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FRAME_REPLICATION_STATE_H_
#define CONTENT_COMMON_FRAME_REPLICATION_STATE_H_

#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

// Sandboxing flags for iframes.  These flags are set via an iframe's "sandbox"
// attribute in the renderer process and forwarded to the browser process,
// which replicates them to other processes as needed.  For a list of sandbox
// flags, see
// http://www.whatwg.org/specs/web-apps/current-work/#attr-iframe-sandbox
// Must be kept in sync with blink::WebSandboxFlags. Enforced in
// render_frame_impl.cc.
enum class SandboxFlags : int {
  NONE = 0,
  NAVIGATION = 1,
  PLUGINS = 1 << 1,
  ORIGIN = 1 << 2,
  FORMS = 1 << 3,
  SCRIPTS = 1 << 4,
  TOP_NAVIGATION = 1 << 5,
  POPUPS = 1 << 6,
  AUTOMATIC_FEATURES = 1 << 7,
  POINTER_LOCK = 1 << 8,
  DOCUMENT_DOMAIN = 1 << 9,
  ORIENTATION_LOCK = 1 << 10,
  ALL = -1
};

inline SandboxFlags operator&(SandboxFlags a, SandboxFlags b) {
  return static_cast<SandboxFlags>(static_cast<int>(a) & static_cast<int>(b));
}

inline SandboxFlags operator~(SandboxFlags flags) {
  return static_cast<SandboxFlags>(~static_cast<int>(flags));
}

// This structure holds information that needs to be replicated between a
// RenderFrame and any of its associated RenderFrameProxies.
struct CONTENT_EXPORT FrameReplicationState {
  FrameReplicationState();
  FrameReplicationState(const std::string& name);
  ~FrameReplicationState();

  // Current serialized security origin of the frame. Unique origins are
  // represented as the string "null" per RFC 6454.
  url::Origin origin;

  // Current sandbox flags of the frame.
  SandboxFlags sandbox_flags;

  // The assigned name of the frame. This name can be empty, unlike the unique
  // name generated internally in the DOM tree.
  std::string name;

  // TODO(alexmos): Eventually, this structure can also hold other state that
  // needs to be replicated, such as frame sizing info.
};

}  // namespace content

#endif  // CONTENT_COMMON_FRAME_REPLICATION_STATE_H_
