// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_FRAME_IMPL_H_
#define CONTENT_RENDERER_RENDER_FRAME_IMPL_H_

#include "base/basictypes.h"
#include "content/public/renderer/render_frame.h"
#include "ipc/ipc_message.h"

namespace content {

class RenderViewImpl;

class CONTENT_EXPORT RenderFrameImpl : public RenderFrame {
 public:
  RenderFrameImpl(RenderViewImpl* render_view, int routing_id);
  virtual ~RenderFrameImpl();

  // IPC::Sender
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // IPC::Listener
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  int routing_id() { return routing_id_; }

 private:
  RenderViewImpl* render_view_;
  int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameImpl);
};

}  // namespace content

#endif // CONTENT_RENDERER_RENDER_FRAME_IMPL_H_
