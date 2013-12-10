// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_DELEGATE_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_DELEGATE_H_

#include "content/common/content_export.h"

namespace IPC {
class Message;
}

namespace content {
class RenderFrameHost;

// An interface implemented by an object interested in knowing about the state
// of the RenderFrameHost.
class CONTENT_EXPORT RenderFrameHostDelegate {
 public:
  // This is used to give the delegate a chance to filter IPC messages.
  virtual bool OnMessageReceived(RenderFrameHost* render_frame_host,
                                 const IPC::Message& message);

  // Informs the delegate whenever a RenderFrameHost is created.
  virtual void RenderFrameCreated(RenderFrameHost* render_frame_host) {}

  // Informs the delegate whenever a RenderFrameHost is deleted.
  virtual void RenderFrameDeleted(RenderFrameHost* render_frame_host) {}

 protected:
  virtual ~RenderFrameHostDelegate() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_DELEGATE_H_
