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
  FrameReplicationState(const std::string& name, SandboxFlags sandbox_flags);
  ~FrameReplicationState();

  // Current serialized security origin of the frame.  Unique origins are
  // represented as the string "null" per RFC 6454.  This field is updated
  // whenever a frame navigation commits.
  //
  // TODO(alexmos): For now, |origin| updates are immediately sent to all frame
  // proxies when in --site-per-process mode. This isn't ideal, since Blink
  // typically needs a proxy's origin only when performing security checks on
  // the ancestors of a local frame.  So, as a future improvement, we could
  // delay sending origin updates to proxies until they have a local descendant
  // (if ever). This would reduce leaking a user's browsing history into a
  // compromized renderer.
  url::Origin origin;

  // Current sandbox flags of the frame.  |sandbox_flags| are initialized for
  // new child frames using the value of the <iframe> element's "sandbox"
  // attribute. They are updated dynamically whenever a parent frame updates an
  // <iframe>'s sandbox attribute via JavaScript.
  //
  // Updates to |sandbox_flags| are sent to proxies, but only after a
  // subsequent navigation of the (sandboxed) frame, since the flags only take
  // effect on navigation (see also FrameTreeNode::effective_sandbox_flags_).
  // The proxies need updated flags so that they can be inherited properly if a
  // proxy ever becomes a parent of a local frame.
  SandboxFlags sandbox_flags;

  // The assigned name of the frame. This name can be empty, unlike the unique
  // name generated internally in the DOM tree.
  //
  // |name| is set when a new child frame is created using the value of the
  // <iframe> element's "name" attribute (see
  // RenderFrameHostImpl::OnCreateChildFrame), and it is updated dynamically
  // whenever a frame sets its window.name.
  //
  // |name| updates are immediately sent to all frame proxies (when in
  // --site-per-process mode), so that other frames can look up or navigate a
  // frame using its updated name (e.g., using window.open(url, frame_name)).
  std::string name;

  // TODO(alexmos): Eventually, this structure can also hold other state that
  // needs to be replicated, such as frame sizing info.
};

}  // namespace content

#endif  // CONTENT_COMMON_FRAME_REPLICATION_STATE_H_
