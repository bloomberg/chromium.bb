// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_RENDER_FRAME_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_RENDER_FRAME_DEVTOOLS_AGENT_HOST_H_

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/devtools/ipc_devtools_agent_host.h"
#include "content/common/content_export.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"

namespace cc {
class CompositorFrameMetadata;
}

namespace content {

class BrowserContext;
class DevToolsFrameTraceRecorder;
class DevToolsProtocolHandler;
class RenderFrameHost;
class RenderFrameHostImpl;

#if defined(OS_ANDROID)
class PowerSaveBlockerImpl;
#endif

namespace devtools {
namespace dom { class DOMHandler; }
namespace emulation { class EmulationHandler; }
namespace input { class InputHandler; }
namespace inspector { class InspectorHandler; }
namespace network { class NetworkHandler; }
namespace page { class PageHandler; }
namespace power { class PowerHandler; }
namespace service_worker { class ServiceWorkerHandler; }
namespace tracing { class TracingHandler; }
}

class CONTENT_EXPORT RenderFrameDevToolsAgentHost
    : public IPCDevToolsAgentHost,
      private WebContentsObserver,
      public NotificationObserver {
 public:
  static void AddAllAgentHosts(DevToolsAgentHost::List* result);

  static void OnCancelPendingNavigation(RenderFrameHost* pending,
                                        RenderFrameHost* current);

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

 private:
  friend class DevToolsAgentHost;
  explicit RenderFrameDevToolsAgentHost(RenderFrameHost*);
  ~RenderFrameDevToolsAgentHost() override;

  static scoped_refptr<DevToolsAgentHost> GetOrCreateFor(RenderFrameHost* host);
  static void AppendAgentHostForFrameIfApplicable(
      DevToolsAgentHost::List* result,
      RenderFrameHost* host);

  // IPCDevToolsAgentHost overrides.
  void DispatchProtocolMessage(const std::string& message) override;
  void SendMessageToAgent(IPC::Message* msg) override;
  void OnClientAttached() override;
  void OnClientDetached() override;

  // WebContentsObserver overrides.
  void AboutToNavigateRenderFrame(RenderFrameHost* old_host,
                                  RenderFrameHost* new_host) override;
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override;
  void FrameDeleted(RenderFrameHost* rfh) override;
  void RenderProcessGone(base::TerminationStatus status) override;
  bool OnMessageReceived(const IPC::Message& message,
                         RenderFrameHost* render_frame_host) override;
  bool OnMessageReceived(const IPC::Message& message) override;
  void DidAttachInterstitialPage() override;
  void DidDetachInterstitialPage() override;
  void TitleWasSet(NavigationEntry* entry, bool explicit_set) override;
  void NavigationEntryCommitted(
      const LoadCommittedDetails& load_details) override;
  void DidCommitProvisionalLoadForFrame(
      RenderFrameHost* render_frame_host,
      const GURL& url,
      ui::PageTransition transition_type) override;

  // NotificationObserver overrides:
  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override;

  void DisconnectRenderFrameHost();
  void ConnectRenderFrameHost(RenderFrameHost* rvh);
  void ReattachToRenderFrameHost(RenderFrameHost* rvh);

  void SetRenderFrameHost(RenderFrameHost* rvh);
  void ClearRenderFrameHost();

  void RenderFrameCrashed();
  void OnSwapCompositorFrame(const IPC::Message& message);
  bool OnSetTouchEventEmulationEnabled(const IPC::Message& message);

  void OnDispatchOnInspectorFrontend(const DevToolsMessageChunk& message);
  void DispatchOnInspectorFrontend(const std::string& message);

  void ClientDetachedFromRenderer();

  void InnerOnClientAttached();
  void InnerClientDetachedFromRenderer();

  bool IsChildFrame();

  RenderFrameHostImpl* render_frame_host_;
  scoped_ptr<devtools::dom::DOMHandler> dom_handler_;
  scoped_ptr<devtools::input::InputHandler> input_handler_;
  scoped_ptr<devtools::inspector::InspectorHandler> inspector_handler_;
  scoped_ptr<devtools::network::NetworkHandler> network_handler_;
  scoped_ptr<devtools::page::PageHandler> page_handler_;
  scoped_ptr<devtools::power::PowerHandler> power_handler_;
  scoped_ptr<devtools::service_worker::ServiceWorkerHandler>
      service_worker_handler_;
  scoped_ptr<devtools::tracing::TracingHandler> tracing_handler_;
  scoped_ptr<devtools::emulation::EmulationHandler> emulation_handler_;
  scoped_ptr<DevToolsProtocolHandler> protocol_handler_;
  scoped_ptr<DevToolsFrameTraceRecorder> frame_trace_recorder_;
#if defined(OS_ANDROID)
  scoped_ptr<PowerSaveBlockerImpl> power_save_blocker_;
#endif
  NotificationRegistrar registrar_;
  bool reattaching_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameDevToolsAgentHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_RENDER_FRAME_DEVTOOLS_AGENT_HOST_H_
