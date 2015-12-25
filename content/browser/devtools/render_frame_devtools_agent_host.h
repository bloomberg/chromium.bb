// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_RENDER_FRAME_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_RENDER_FRAME_DEVTOOLS_AGENT_HOST_H_

#include <map>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"

namespace cc {
class CompositorFrameMetadata;
}

namespace content {

class BrowserContext;
class DevToolsFrameTraceRecorder;
class DevToolsProtocolHandler;
class FrameTreeNode;
class NavigationHandle;
class RenderFrameHostImpl;

#if defined(OS_ANDROID)
class PowerSaveBlockerImpl;
#endif

namespace devtools {
namespace dom { class DOMHandler; }
namespace emulation { class EmulationHandler; }
namespace input { class InputHandler; }
namespace inspector { class InspectorHandler; }
namespace io { class IOHandler; }
namespace network { class NetworkHandler; }
namespace page { class PageHandler; }
namespace security { class SecurityHandler; }
namespace service_worker { class ServiceWorkerHandler; }
namespace tracing { class TracingHandler; }
}

class CONTENT_EXPORT RenderFrameDevToolsAgentHost
    : public DevToolsAgentHostImpl,
      private WebContentsObserver {
 public:
  static void AddAllAgentHosts(DevToolsAgentHost::List* result);

  static void OnCancelPendingNavigation(RenderFrameHost* pending,
                                        RenderFrameHost* current);
  static void OnBeforeNavigation(RenderFrameHost* current,
                                 RenderFrameHost* pending);

  void SynchronousSwapCompositorFrame(
      const cc::CompositorFrameMetadata& frame_metadata);

  bool HasRenderFrameHost(RenderFrameHost* host);

  // DevTooolsAgentHost overrides.
  void DisconnectWebContents() override;
  void ConnectWebContents(WebContents* web_contents) override;
  BrowserContext* GetBrowserContext() override;
  WebContents* GetWebContents() override;
  Type GetType() override;
  std::string GetTitle() override;
  GURL GetURL() override;
  bool Activate() override;
  bool Close() override;
  bool DispatchProtocolMessage(const std::string& message) override;

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
  void Attach() override;
  void Detach() override;
  void InspectElement(int x, int y) override;

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
  bool OnMessageReceived(const IPC::Message& message) override;
  void DidAttachInterstitialPage() override;
  void DidDetachInterstitialPage() override;
  void DidCommitProvisionalLoadForFrame(
      RenderFrameHost* render_frame_host,
      const GURL& url,
      ui::PageTransition transition_type) override;
  void DidFailProvisionalLoad(
      RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      int error_code,
      const base::string16& error_description,
      bool was_ignored_by_handler) override;

  void AboutToNavigateRenderFrame(RenderFrameHost* old_host,
                                  RenderFrameHost* new_host);

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
  void DestroyOnRenderFrameGone();

  bool MatchesMyTreeNode(NavigationHandle* navigation_handle);

  class FrameHostHolder;

  scoped_ptr<FrameHostHolder> current_;
  scoped_ptr<FrameHostHolder> pending_;

  // Stores per-host state between DisconnectWebContents and ConnectWebContents.
  scoped_ptr<FrameHostHolder> disconnected_;

  scoped_ptr<devtools::dom::DOMHandler> dom_handler_;
  scoped_ptr<devtools::input::InputHandler> input_handler_;
  scoped_ptr<devtools::inspector::InspectorHandler> inspector_handler_;
  scoped_ptr<devtools::io::IOHandler> io_handler_;
  scoped_ptr<devtools::network::NetworkHandler> network_handler_;
  scoped_ptr<devtools::page::PageHandler> page_handler_;
  scoped_ptr<devtools::security::SecurityHandler> security_handler_;
  scoped_ptr<devtools::service_worker::ServiceWorkerHandler>
      service_worker_handler_;
  scoped_ptr<devtools::tracing::TracingHandler> tracing_handler_;
  scoped_ptr<devtools::emulation::EmulationHandler> emulation_handler_;
  scoped_ptr<DevToolsFrameTraceRecorder> frame_trace_recorder_;
#if defined(OS_ANDROID)
  scoped_ptr<PowerSaveBlockerImpl> power_save_blocker_;
#endif
  scoped_ptr<DevToolsProtocolHandler> protocol_handler_;
  bool current_frame_crashed_;

  // PlzNavigate

  // Handle that caused the setting of pending_.
  NavigationHandle* pending_handle_;

  // Navigation counter and queue for buffering protocol messages during a
  // navigation.
  int in_navigation_;

  // <call_id> -> <session_id, message>
  std::map<int, std::pair<int, std::string>>
      in_navigation_protocol_message_buffer_;

  // The FrameTreeNode associated with this agent.
  FrameTreeNode* frame_tree_node_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameDevToolsAgentHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_RENDER_FRAME_DEVTOOLS_AGENT_HOST_H_
