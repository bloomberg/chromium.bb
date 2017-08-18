// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_RENDER_FRAME_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_RENDER_FRAME_DEVTOOLS_AGENT_HOST_H_

#include <map>
#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/net_errors.h"

#if defined(OS_ANDROID)
#include "ui/android/view_android.h"
#endif  // OS_ANDROID

namespace cc {
class CompositorFrameMetadata;
}

namespace net {
class HttpRequestHeaders;
}

#if defined(OS_ANDROID)
#include "services/device/public/interfaces/wake_lock.mojom.h"
#endif

namespace content {

class BrowserContext;
class DevToolsFrameTraceRecorder;
class FrameTreeNode;
class NavigationHandle;
class NavigationHandleImpl;
class NavigationThrottle;
class RenderFrameHostImpl;
struct BeginNavigationParams;
struct CommonNavigationParams;

class CONTENT_EXPORT RenderFrameDevToolsAgentHost
    : public DevToolsAgentHostImpl,
      private WebContentsObserver {
 public:
  static void AddAllAgentHosts(DevToolsAgentHost::List* result);
  static scoped_refptr<DevToolsAgentHost> GetOrCreateFor(
      FrameTreeNode* frame_tree_node);

  static void OnCancelPendingNavigation(RenderFrameHost* pending,
                                        RenderFrameHost* current);
  static void OnBeforeNavigation(RenderFrameHost* current,
                                 RenderFrameHost* pending);
  static void OnFailedNavigation(RenderFrameHost* host,
                                 const CommonNavigationParams& common_params,
                                 const BeginNavigationParams& begin_params,
                                 net::Error error_code);
  static std::unique_ptr<NavigationThrottle> CreateThrottleForNavigation(
      NavigationHandle* navigation_handle);
  static bool IsNetworkHandlerEnabled(FrameTreeNode* frame_tree_node);
  static void AppendDevToolsHeaders(FrameTreeNode* frame_tree_node,
                                    net::HttpRequestHeaders* headers);
  static void WebContentsCreated(WebContents* web_contents);

  static void SignalSynchronousSwapCompositorFrame(
      RenderFrameHost* frame_host,
      cc::CompositorFrameMetadata frame_metadata);

  FrameTreeNode* frame_tree_node() { return frame_tree_node_; }

  // DevToolsAgentHost overrides.
  void DisconnectWebContents() override;
  void ConnectWebContents(WebContents* web_contents) override;
  BrowserContext* GetBrowserContext() override;
  WebContents* GetWebContents() override;
  std::string GetParentId() override;
  std::string GetType() override;
  std::string GetTitle() override;
  std::string GetDescription() override;
  GURL GetURL() override;
  GURL GetFaviconURL() override;
  bool Activate() override;
  void Reload() override;

  bool Close() override;
  base::TimeTicks GetLastActivityTime() override;

 private:
  friend class DevToolsAgentHost;
  explicit RenderFrameDevToolsAgentHost(FrameTreeNode*);
  ~RenderFrameDevToolsAgentHost() override;

  // DevToolsAgentHostImpl overrides.
  void AttachSession(DevToolsSession* session) override;
  void DetachSession(int session_id) override;
  void InspectElement(DevToolsSession* session, int x, int y) override;
  bool DispatchProtocolMessage(
      DevToolsSession* session,
      const std::string& message) override;

  // WebContentsObserver overrides.
  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override;
  void FrameDeleted(RenderFrameHost* rfh) override;
  void RenderFrameDeleted(RenderFrameHost* rfh) override;
  void RenderProcessGone(base::TerminationStatus status) override;
  bool OnMessageReceived(const IPC::Message& message,
                         RenderFrameHost* render_frame_host) override;
  void DidAttachInterstitialPage() override;
  void DidDetachInterstitialPage() override;
  void WasShown() override;
  void WasHidden() override;
  void DidReceiveCompositorFrame() override;

  void AboutToNavigateRenderFrame(RenderFrameHost* old_host,
                                  RenderFrameHost* new_host);

  void SetPending(RenderFrameHostImpl* host);
  void CommitPending();
  void DiscardPending();
  void UpdateProtocolHandlers(RenderFrameHostImpl* host);

  bool IsChildFrame();

  void OnClientsAttached();
  void OnClientsDetached();

  void RenderFrameCrashed();
  void OnSwapCompositorFrame(const IPC::Message& message);
  void OnDispatchOnInspectorFrontend(
      RenderFrameHost* sender,
      const DevToolsMessageChunk& message);
  void OnRequestNewWindow(RenderFrameHost* sender, int new_routing_id);
  void DestroyOnRenderFrameGone();

  bool CheckConsistency();
  void UpdateFrameHost(RenderFrameHostImpl* frame_host);
  void MaybeReattachToRenderFrame();
  void GrantPolicy(RenderFrameHostImpl* host);
  void RevokePolicy(RenderFrameHostImpl* host);

#if defined(OS_ANDROID)
  device::mojom::WakeLock* GetWakeLock();
#endif

  void SynchronousSwapCompositorFrame(
      cc::CompositorFrameMetadata frame_metadata);

  class FrameHostHolder;

  std::unique_ptr<FrameHostHolder> current_;
  std::unique_ptr<FrameHostHolder> pending_;

  // Stores per-host state between DisconnectWebContents and ConnectWebContents.
  base::flat_map<int, std::string> disconnected_cookie_for_session_;

  std::unique_ptr<DevToolsFrameTraceRecorder> frame_trace_recorder_;
#if defined(OS_ANDROID)
  device::mojom::WakeLockPtr wake_lock_;
#endif
  RenderFrameHostImpl* handlers_frame_host_;
  bool current_frame_crashed_;

  // PlzNavigate

  // The active host we are talking to.
  RenderFrameHostImpl* frame_host_ = nullptr;
  base::flat_set<NavigationHandleImpl*> navigation_handles_;
  bool render_frame_alive_ = false;

  // These messages were queued after suspending, not sent to the agent,
  // and will be sent after resuming.
  struct Message {
    int call_id;
    std::string method;
    std::string message;
  };
  std::map<int, std::vector<Message>> suspended_messages_by_session_id_;

  // The FrameTreeNode associated with this agent.
  FrameTreeNode* frame_tree_node_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameDevToolsAgentHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_RENDER_FRAME_DEVTOOLS_AGENT_HOST_H_
