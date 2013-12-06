// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDER_FRAME_H_
#define CONTENT_PUBLIC_RENDERER_RENDER_FRAME_H_

#include "content/common/content_export.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"

namespace blink {
class WebFrame;
class WebPlugin;
struct WebPluginParams;
}

namespace content {
struct WebPluginInfo;

// This interface wraps functionality, which is specific to frames, such as
// navigation. It provides communication with a corresponding RenderFrameHost
// in the browser process.
class CONTENT_EXPORT RenderFrame : public IPC::Listener,
                                   public IPC::Sender {
 public:
  // Get the routing ID of the frame.
  virtual int GetRoutingID() = 0;

  // Create a new NPAPI/Pepper plugin depending on |info|. Returns NULL if no
  // plugin was found.
  virtual blink::WebPlugin* CreatePlugin(
      blink::WebFrame* frame,
      const WebPluginInfo& info,
      const blink::WebPluginParams& params) = 0;

 protected:
  virtual ~RenderFrame() {}

 private:
  // This interface should only be implemented inside content.
  friend class RenderFrameImpl;
  RenderFrame() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDER_FRAME_H_
