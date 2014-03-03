// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_IMPL_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_IMPL_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_frame_host.h"

class GURL;
struct FrameHostMsg_DidFailProvisionalLoadWithError_Params;
struct FrameMsg_Navigate_Params;

namespace base {
class FilePath;
}

namespace content {

class CrossProcessFrameConnector;
class FrameTree;
class FrameTreeNode;
class RenderFrameHostDelegate;
class RenderProcessHost;
class RenderViewHostImpl;
struct ContextMenuParams;

class CONTENT_EXPORT RenderFrameHostImpl : public RenderFrameHost {
 public:
  static RenderFrameHostImpl* FromID(int process_id, int routing_id);

  virtual ~RenderFrameHostImpl();

  // RenderFrameHost
  virtual int GetRoutingID() OVERRIDE;
  virtual SiteInstance* GetSiteInstance() OVERRIDE;
  virtual RenderProcessHost* GetProcess() OVERRIDE;
  virtual RenderFrameHost* GetParent() OVERRIDE;
  virtual bool IsCrossProcessSubframe() OVERRIDE;
  virtual GURL GetLastCommittedURL() OVERRIDE;
  virtual gfx::NativeView GetNativeView() OVERRIDE;
  virtual void NotifyContextMenuClosed(
      const CustomContextMenuContext& context) OVERRIDE;
  virtual void ExecuteCustomContextMenuCommand(
      int action, const CustomContextMenuContext& context) OVERRIDE;
  virtual RenderViewHost* GetRenderViewHost() OVERRIDE;

  // IPC::Sender
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // IPC::Listener
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  void Init();
  int routing_id() const { return routing_id_; }
  void OnCreateChildFrame(int new_routing_id,
                          const std::string& frame_name);

  RenderViewHostImpl* render_view_host() { return render_view_host_; }
  RenderFrameHostDelegate* delegate() { return delegate_; }
  FrameTreeNode* frame_tree_node() { return frame_tree_node_; }

  // This function is called when this is a swapped out RenderFrameHost that
  // lives in the same process as the parent frame. The
  // |cross_process_frame_connector| allows the non-swapped-out
  // RenderFrameHost for a frame to communicate with the parent process
  // so that it may composite drawing data.
  //
  // Ownership is not transfered.
  void set_cross_process_frame_connector(
      CrossProcessFrameConnector* cross_process_frame_connector) {
    cross_process_frame_connector_ = cross_process_frame_connector;
  }

  // Returns a bitwise OR of bindings types that have been enabled for this
  // RenderFrameHostImpl's RenderView. See BindingsPolicy for details.
  // TODO(creis): Make bindings frame-specific, to support cases like <webview>.
  int GetEnabledBindings();

  // Hack to get this subframe to swap out, without yet moving over all the
  // SwapOut state and machinery from RenderViewHost.
  void SwapOut();
  void OnSwappedOut(bool timed_out);
  bool is_swapped_out() { return is_swapped_out_; }
  void set_swapped_out(bool is_swapped_out) {
    is_swapped_out_ = is_swapped_out;
  }

  // Sets the RVH for |this| as pending shutdown. |on_swap_out| will be called
  // when the SwapOutACK is received.
  void SetPendingShutdown(const base::Closure& on_swap_out);

  // TODO(nasko): This method is public so RenderViewHostImpl::Navigate can
  // call it directly. It should be made private once Navigate moves here.
  void OnDidStartLoading();

  // Sends the given navigation message. Use this rather than sending it
  // yourself since this does the internal bookkeeping described below. This
  // function takes ownership of the provided message pointer.
  //
  // If a cross-site request is in progress, we may be suspended while waiting
  // for the onbeforeunload handler, so this function might buffer the message
  // rather than sending it.
  void Navigate(const FrameMsg_Navigate_Params& params);

  // Load the specified URL; this is a shortcut for Navigate().
  void NavigateToURL(const GURL& url);

 protected:
  friend class RenderFrameHostFactory;

  // TODO(nasko): Remove dependency on RenderViewHost here. RenderProcessHost
  // should be the abstraction needed here, but we need RenderViewHost to pass
  // into WebContentsObserver::FrameDetached for now.
  RenderFrameHostImpl(RenderViewHostImpl* render_view_host,
                      RenderFrameHostDelegate* delegate,
                      FrameTree* frame_tree,
                      FrameTreeNode* frame_tree_node,
                      int routing_id,
                      bool is_swapped_out);

 private:
  friend class TestRenderFrameHost;
  friend class TestRenderViewHost;

  // IPC Message handlers.
  void OnDetach();
  void OnDidStartProvisionalLoadForFrame(int parent_routing_id,
                                         bool main_frame,
                                         const GURL& url);
  void OnDidFailProvisionalLoadWithError(
      const FrameHostMsg_DidFailProvisionalLoadWithError_Params& params);
  void OnDidFailLoadWithError(
      const GURL& url,
      bool is_main_frame,
      int error_code,
      const base::string16& error_description);
  void OnDidRedirectProvisionalLoad(int32 page_id,
                                    const GURL& source_url,
                                    const GURL& target_url);
  void OnNavigate(const IPC::Message& msg);
  void OnDidStopLoading();
  void OnSwapOutACK();
  void OnContextMenu(const ContextMenuParams& params);

  // Returns whether the given URL is allowed to commit in the current process.
  // This is a more conservative check than RenderProcessHost::FilterURL, since
  // it will be used to kill processes that commit unauthorized URLs.
  bool CanCommitURL(const GURL& url);

  // For now, RenderFrameHosts indirectly keep RenderViewHosts alive via a
  // refcount that calls Shutdown when it reaches zero.  This allows each
  // RenderFrameHostManager to just care about RenderFrameHosts, while ensuring
  // we have a RenderViewHost for each RenderFrameHost.
  // TODO(creis): RenderViewHost will eventually go away and be replaced with
  // some form of page context.
  RenderViewHostImpl* render_view_host_;

  RenderFrameHostDelegate* delegate_;

  // |cross_process_frame_connector_| passes messages from an out-of-process
  // child frame to the parent process for compositing.
  //
  // This is only non-NULL when this is the swapped out RenderFrameHost in
  // the same site instance as this frame's parent.
  //
  // See the class comment above CrossProcessFrameConnector for more
  // information.
  //
  // This will move to RenderFrameProxyHost when that class is created.
  CrossProcessFrameConnector* cross_process_frame_connector_;

  // Reference to the whole frame tree that this RenderFrameHost belongs to.
  // Allows this RenderFrameHost to add and remove nodes in response to
  // messages from the renderer requesting DOM manipulation.
  FrameTree* frame_tree_;

  // The FrameTreeNode which this RenderFrameHostImpl is hosted in.
  FrameTreeNode* frame_tree_node_;

  int routing_id_;
  bool is_swapped_out_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_IMPL_H_
