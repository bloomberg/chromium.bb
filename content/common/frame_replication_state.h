// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FRAME_REPLICATION_STATE_H_
#define CONTENT_COMMON_FRAME_REPLICATION_STATE_H_

#include "content/common/content_export.h"
#include "url/origin.h"

namespace blink {
enum class WebTreeScopeType;
enum class WebSandboxFlags;
}

namespace content {

// This structure holds information that needs to be replicated between a
// RenderFrame and any of its associated RenderFrameProxies.
struct CONTENT_EXPORT FrameReplicationState {
  FrameReplicationState();
  FrameReplicationState(blink::WebTreeScopeType scope,
                        const std::string& name,
                        blink::WebSandboxFlags sandbox_flags,
                        bool should_enforce_strict_mixed_content_checking);
  ~FrameReplicationState();

  // Current origin of the frame. This field is updated whenever a frame
  // navigation commits.
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
  blink::WebSandboxFlags sandbox_flags;

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

  // Whether the frame is in a document tree or a shadow tree, per the Shadow
  // DOM spec: https://w3c.github.io/webcomponents/spec/shadow/
  // Note: This should really be const, as it can never change once a frame is
  // created. However, making it const makes it a pain to embed into IPC message
  // params: having a const member implicitly deletes the copy assignment
  // operator.
  blink::WebTreeScopeType scope;

  // True if a frame's current document should strictly block all mixed
  // content. Updates are immediately sent to all frame proxies when
  // frames live in different processes.
  bool should_enforce_strict_mixed_content_checking;

  // TODO(alexmos): Eventually, this structure can also hold other state that
  // needs to be replicated, such as frame sizing info.
};

}  // namespace content

#endif  // CONTENT_COMMON_FRAME_REPLICATION_STATE_H_
