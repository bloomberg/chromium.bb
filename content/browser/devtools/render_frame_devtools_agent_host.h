// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_RENDER_FRAME_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_RENDER_FRAME_DEVTOOLS_AGENT_HOST_H_

#include <map>
#include <memory>

#include "base/compiler_specific.h"
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

#if defined(OS_ANDROID)
namespace device {
class PowerSaveBlocker;
}  // namespace device
#endif

namespace content {

class BrowserContext;
class DevToolsFrameTraceRecorder;
class FrameTreeNode;
class NavigationHandle;
class NavigationThrottle;
class RenderFrameHostImpl;
struct BeginNavigationParams;
struct CommonNavigationParams;

class CONTENT_EXPORT RenderFrameDevToolsAgentHost
    : public DevToolsAgentHostImpl,
      private WebContentsObserver {
 public:
  static void AddAllAgentHosts(DevToolsAgentHost::List* result);

  static void OnCancelPendingNavigation(RenderFrameHost* pending,
                                        RenderFrameHost* current);
  static void OnBeforeNavigation(RenderFrameHost* current,
                                 RenderFrameHost* pending);
  static void OnBeforeNavigation(NavigationHandle* navigation_handle);
  static void OnFailedNavigation(RenderFrameHost* host,
                                 const CommonNavigationParams& common_params,
                                 const BeginNavigationParams& begin_params,
                                 net::Error error_code);
  static std::unique_ptr<NavigationThrottle> CreateThrottleForNavigation(
      NavigationHandle* navigation_handle);
  static bool IsNetworkHandlerEnabled(FrameTreeNode* frame_tree_node);
  static std::string UserAgentOverride(FrameTreeNode* frame_tree_node);

  static void WebContentsCreated(WebContents* web_contents);

  static void SignalSynchronousSwapCompositorFrame(
      RenderFrameHost* frame_host,
      cc::CompositorFrameMetadata frame_metadata);

  bool HasRenderFrameHost(RenderFrameHost* host);

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
  explicit RenderFrameDevToolsAgentHost(RenderFrameHostImpl*);
  ~RenderFrameDevToolsAgentHost() override;

  static scoped_refptr<DevToolsAgentHost> GetOrCreateFor(
      RenderFrameHostImpl* host);
  static void AppendAgentHostForFrameIfApplicable(
      DevToolsAgentHost::List* result,
      RenderFrameHost* host);

  // DevToolsAgentHostImpl overrides.
  void AttachSession(DevToolsSession* session) override;
  void DetachSession(int session_id) override;
  void InspectElement(DevToolsSession* session, int x, int y) override;
  bool DispatchProtocolMessage(
      DevToolsSession* session,
      const std::string& message) override;

  // WebContentsObserver overrides.
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
  void AboutToNavigate(NavigationHandle* navigation_handle);
  void OnFailedNavigation(const CommonNavigationParams& common_params,
                          const BeginNavigationParams& begin_params,
                          net::Error error_code);

  void DispatchBufferedProtocolMessagesIfNecessary();

  void SetPending(RenderFrameHostImpl* host);
  void CommitPending();
  void DiscardPending();
  void UpdateProtocolHandlers(RenderFrameHostImpl* host);

  bool IsChildFrame();

  void OnClientAttached();
  void OnClientDetached();

  void RenderFrameCrashed();
  void OnSwapCompositorFrame(const IPC::Message& message);
  void OnDispatchOnInspectorFrontend(
      RenderFrameHost* sender,
      const DevToolsMessageChunk& message);
  void OnRequestNewWindow(RenderFrameHost* sender, int new_routing_id);
  void DestroyOnRenderFrameGone();

  bool CheckConsistency();

  void CreatePowerSaveBlocker();

  void SynchronousSwapCompositorFrame(
      cc::CompositorFrameMetadata frame_metadata);

  class FrameHostHolder;

  std::unique_ptr<FrameHostHolder> current_;
  std::unique_ptr<FrameHostHolder> pending_;

  // Stores per-host state between DisconnectWebContents and ConnectWebContents.
  std::unique_ptr<FrameHostHolder> disconnected_;

  std::unique_ptr<DevToolsFrameTraceRecorder> frame_trace_recorder_;
#if defined(OS_ANDROID)
  std::unique_ptr<device::PowerSaveBlocker> power_save_blocker_;
#endif
  RenderFrameHostImpl* handlers_frame_host_;
  bool current_frame_crashed_;

  // PlzNavigate

  // Handle that caused the setting of pending_.
  NavigationHandle* pending_handle_;

  // List of handles currently navigating.
  std::set<NavigationHandle*> navigating_handles_;

  struct PendingMessage {
    int session_id;
    std::string method;
    std::string message;
  };
  // <call_id> -> PendingMessage
  std::map<int, PendingMessage> in_navigation_protocol_message_buffer_;

  // The FrameTreeNode associated with this agent.
  FrameTreeNode* frame_tree_node_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameDevToolsAgentHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_RENDER_FRAME_DEVTOOLS_AGENT_HOST_H_
