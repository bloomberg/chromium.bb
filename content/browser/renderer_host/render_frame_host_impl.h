// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_IMPL_H_

#include "base/compiler_specific.h"
#include "content/public/browser/render_frame_host.h"

namespace content {

class RenderViewHostImpl;

class CONTENT_EXPORT RenderFrameHostImpl : public RenderFrameHost {
 public:
  RenderFrameHostImpl(
      RenderViewHostImpl* render_view_host,
      int routing_id,
      bool swapped_out);
  virtual ~RenderFrameHostImpl();

  // IPC::Sender
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // IPC::Listener
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  int routing_id() { return routing_id_; }

 private:
  RenderViewHostImpl* render_view_host_;

  int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_IMPL_H_
