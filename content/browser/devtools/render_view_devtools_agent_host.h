// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_RENDER_VIEW_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_RENDER_VIEW_DEVTOOLS_AGENT_HOST_H_

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

class DevToolsProtocolHandlerImpl;
class RenderViewHost;
class RenderViewHostImpl;

#if defined(OS_ANDROID)
class PowerSaveBlockerImpl;
#endif

namespace devtools {
namespace dom { class DOMHandler; }
namespace input { class InputHandler; }
namespace network { class NetworkHandler; }
namespace page { class PageHandler; }
namespace power { class PowerHandler; }
namespace tracing { class TracingHandler; }
}

class CONTENT_EXPORT RenderViewDevToolsAgentHost
    : public IPCDevToolsAgentHost,
      private WebContentsObserver,
      public NotificationObserver {
 public:
  static void OnCancelPendingNavigation(RenderViewHost* pending,
                                        RenderViewHost* current);

  RenderViewDevToolsAgentHost(RenderViewHost*);

  void SynchronousSwapCompositorFrame(
      const cc::CompositorFrameMetadata& frame_metadata);

  // DevTooolsAgentHost overrides.
  void DisconnectWebContents() override;
  void ConnectWebContents(WebContents* web_contents) override;
  WebContents* GetWebContents() override;
  Type GetType() override;
  std::string GetTitle() override;
  GURL GetURL() override;
  bool Activate() override;
  bool Close() override;

 private:
  friend class DevToolsAgentHost;
  ~RenderViewDevToolsAgentHost() override;

  // IPCDevToolsAgentHost overrides.
  void DispatchProtocolMessage(const std::string& message) override;
  void SendMessageToAgent(IPC::Message* msg) override;
  void OnClientAttached() override;
  void OnClientDetached() override;

  // WebContentsObserver overrides.
  void AboutToNavigateRenderView(RenderViewHost* dest_rvh) override;
  void RenderViewHostChanged(RenderViewHost* old_host,
                             RenderViewHost* new_host) override;
  void RenderViewDeleted(RenderViewHost* rvh) override;
  void RenderProcessGone(base::TerminationStatus status) override;
  bool OnMessageReceived(const IPC::Message& message,
                         RenderFrameHost* render_frame_host) override;
  bool OnMessageReceived(const IPC::Message& message) override;
  void DidAttachInterstitialPage() override;
  void DidDetachInterstitialPage() override;
  void TitleWasSet(NavigationEntry* entry, bool explicit_set) override;
  void NavigationEntryCommitted(
      const LoadCommittedDetails& load_details) override;

  // NotificationObserver overrides:
  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override;

  void DisconnectRenderViewHost();
  void ConnectRenderViewHost(RenderViewHost* rvh);
  void ReattachToRenderViewHost(RenderViewHost* rvh);

  bool DispatchIPCMessage(const IPC::Message& message);

  void SetRenderViewHost(RenderViewHost* rvh);
  void ClearRenderViewHost();

  void RenderViewCrashed();
  void OnSwapCompositorFrame(const IPC::Message& message);
  bool OnSetTouchEventEmulationEnabled(const IPC::Message& message);

  void OnDispatchOnInspectorFrontend(const std::string& message,
                                     uint32 total_size);
  void DispatchOnInspectorFrontend(const std::string& message);
  void OnSaveAgentRuntimeState(const std::string& state);

  void ClientDetachedFromRenderer();

  void InnerOnClientAttached();
  void InnerClientDetachedFromRenderer();

  RenderViewHostImpl* render_view_host_;
  scoped_ptr<devtools::dom::DOMHandler> dom_handler_;
  scoped_ptr<devtools::input::InputHandler> input_handler_;
  scoped_ptr<devtools::network::NetworkHandler> network_handler_;
  scoped_ptr<devtools::page::PageHandler> page_handler_;
  scoped_ptr<devtools::power::PowerHandler> power_handler_;
  scoped_ptr<devtools::tracing::TracingHandler> tracing_handler_;
  scoped_ptr<DevToolsProtocolHandlerImpl> handler_impl_;
#if defined(OS_ANDROID)
  scoped_ptr<PowerSaveBlockerImpl> power_save_blocker_;
#endif
  std::string state_;
  NotificationRegistrar registrar_;
  bool reattaching_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewDevToolsAgentHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_RENDER_VIEW_DEVTOOLS_AGENT_HOST_H_
