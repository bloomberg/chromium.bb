// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_H_

#include "content/common/content_export.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace content {
class RenderProcessHost;
class RenderViewHost;
class SiteInstance;
struct CustomContextMenuContext;

// The interface provides a communication conduit with a frame in the renderer.
class CONTENT_EXPORT RenderFrameHost : public IPC::Listener,
                                       public IPC::Sender {
 public:
  // Returns the RenderFrameHost given its ID and the ID of its render process.
  // Returns NULL if the IDs do not correspond to a live RenderFrameHost.
  static RenderFrameHost* FromID(int render_process_id, int render_frame_id);

  virtual ~RenderFrameHost() {}

  virtual SiteInstance* GetSiteInstance() = 0;

  // Returns the process for this frame.
  virtual RenderProcessHost* GetProcess() = 0;

  // Returns the route id for this frame.
  virtual int GetRoutingID() = 0;

  // Returns the current RenderFrameHost of the parent frame, or NULL if there
  // is no parent. The result may be in a different process than the current
  // RenderFrameHost.
  virtual RenderFrameHost* GetParent() = 0;

  // Returns true if the frame is out of process.
  virtual bool IsCrossProcessSubframe() = 0;

  // Returns the last committed URL of the frame.
  virtual GURL GetLastCommittedURL() = 0;

  // Returns the associated widget's native view.
  virtual gfx::NativeView GetNativeView() = 0;

  // Let the renderer know that the menu has been closed.
  virtual void NotifyContextMenuClosed(
      const CustomContextMenuContext& context) = 0;

  // Executes custom context menu action that was provided from Blink.
  virtual void ExecuteCustomContextMenuCommand(
      int action, const CustomContextMenuContext& context) = 0;

  // Temporary until we get rid of RenderViewHost.
  virtual RenderViewHost* GetRenderViewHost() = 0;

 private:
  // This interface should only be implemented inside content.
  friend class RenderFrameHostImpl;
  RenderFrameHost() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_H_
