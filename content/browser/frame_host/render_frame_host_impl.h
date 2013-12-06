// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_IMPL_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_IMPL_H_

#include <string>

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_frame_host.h"

class GURL;

namespace base {
class FilePath;
}

namespace content {

class FrameTree;
class RenderFrameHostDelegate;
class RenderProcessHost;
class RenderViewHostImpl;

class CONTENT_EXPORT RenderFrameHostImpl : public RenderFrameHost {
 public:
  static RenderFrameHostImpl* FromID(int process_id, int routing_id);

  virtual ~RenderFrameHostImpl();

  // RenderFrameHost
  virtual int GetRoutingID() OVERRIDE;

  // IPC::Sender
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // IPC::Listener
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  void Init();
  RenderProcessHost* GetProcess() const;
  int routing_id() const { return routing_id_; }
  void OnCreateChildFrame(int new_frame_routing_id,
                          int64 parent_frame_id,
                          int64 frame_id,
                          const std::string& frame_name);

  RenderViewHostImpl* render_view_host() { return render_view_host_; }
  RenderFrameHostDelegate* delegate() { return delegate_; }

 protected:
  friend class RenderFrameHostFactory;

  // TODO(nasko): Remove dependency on RenderViewHost here. RenderProcessHost
  // should be the abstraction needed here, but we need RenderViewHost to pass
  // into WebContentsObserver::FrameDetached for now.
  RenderFrameHostImpl(RenderViewHostImpl* render_view_host,
                      RenderFrameHostDelegate* delegate,
                      FrameTree* frame_tree,
                      int routing_id,
                      bool is_swapped_out);

 private:
  // IPC Message handlers.
  void OnDetach(int64 parent_frame_id, int64 frame_id);
  void OnDidStartProvisionalLoadForFrame(int64 frame_id,
                                         int64 parent_frame_id,
                                         bool main_frame,
                                         const GURL& url);
  void OnPepperPluginHung(int plugin_child_id,
                          const base::FilePath& path,
                          bool is_hung);

  bool is_swapped_out() { return is_swapped_out_; }

  // TODO(nasko): This should be removed and replaced by RenderProcessHost.
  RenderViewHostImpl* render_view_host_;  // Not owned.

  RenderFrameHostDelegate* delegate_;

  // Reference to the whole frame tree that this RenderFrameHost belongs too.
  // Allows this RenderFrameHost to add and remove nodes in response to
  // messages from the renderer requesting DOM manipulation.
  FrameTree* frame_tree_;
  int routing_id_;
  bool is_swapped_out_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_IMPL_H_
