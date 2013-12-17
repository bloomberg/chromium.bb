// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_H_

#include "content/common/content_export.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"

namespace content {
class RenderProcessHost;

// The interface provides a communication conduit with a frame in the renderer.
class CONTENT_EXPORT RenderFrameHost : public IPC::Listener,
                                       public IPC::Sender {
 public:
  // Returns the RenderFrameHost given its ID and the ID of its render process.
  // Returns NULL if the IDs do not correspond to a live RenderFrameHost.
  static RenderFrameHost* FromID(int render_process_id, int render_frame_id);

  virtual ~RenderFrameHost() {}

  // Returns the process for this frame.
  virtual RenderProcessHost* GetProcess() = 0;

  // Returns the route id for this frame.
  virtual int GetRoutingID() = 0;

 private:
  // This interface should only be implemented inside content.
  friend class RenderFrameHostImpl;
  RenderFrameHost() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_H_
