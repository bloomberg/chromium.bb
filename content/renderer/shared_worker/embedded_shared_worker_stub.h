// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SHARED_WORKER_EMBEDDED_SHARED_WORKER_STUB_H_
#define CONTENT_RENDERER_SHARED_WORKER_EMBEDDED_SHARED_WORKER_STUB_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/unguessable_token.h"
#include "content/child/scoped_child_process_reference.h"
#include "content/common/shared_worker/shared_worker.mojom.h"
#include "content/common/shared_worker/shared_worker_host.mojom.h"
#include "content/common/shared_worker/shared_worker_info.mojom.h"
#include "content/renderer/child_message_filter.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/interfaces/interface_provider.mojom.h"
#include "third_party/WebKit/public/platform/WebAddressSpace.h"
#include "third_party/WebKit/public/platform/WebContentSecurityPolicy.h"
#include "third_party/WebKit/public/platform/WebContentSettingsClient.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebSharedWorkerClient.h"
#include "third_party/WebKit/public/web/worker_content_settings_proxy.mojom.h"
#include "url/gurl.h"

namespace blink {
class WebApplicationCacheHost;
class WebApplicationCacheHostClient;
class WebNotificationPresenter;
class WebSharedWorker;
}

namespace blink {
class MessagePortChannel;
}

namespace content {
class SharedWorkerDevToolsAgent;
class WebApplicationCacheHostImpl;

// A stub class to receive IPC from browser process and talk to
// blink::WebSharedWorker. Implements blink::WebSharedWorkerClient.
// This class is self-destruct (no one explicitly owns this), and
// deletes itself (via private Shutdown() method) when either one of
// following methods is called by blink::WebSharedWorker:
// - workerScriptLoadFailed() or
// - workerContextDestroyed()
//
// In either case the corresponding blink::WebSharedWorker also deletes
// itself.
class EmbeddedSharedWorkerStub : public IPC::Listener,
                                 public blink::WebSharedWorkerClient,
                                 public mojom::SharedWorker {
 public:
  EmbeddedSharedWorkerStub(
      mojom::SharedWorkerInfoPtr info,
      bool pause_on_start,
      const base::UnguessableToken& devtools_worker_token,
      int route_id,
      blink::mojom::WorkerContentSettingsProxyPtr content_settings,
      mojom::SharedWorkerHostPtr host,
      mojom::SharedWorkerRequest request,
      service_manager::mojom::InterfaceProviderPtr interface_provider);
  ~EmbeddedSharedWorkerStub() override;

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelError() override;

  // blink::WebSharedWorkerClient implementation.
  void CountFeature(blink::mojom::WebFeature feature) override;
  void WorkerContextClosed() override;
  void WorkerContextDestroyed() override;
  void WorkerReadyForInspection() override;
  void WorkerScriptLoaded() override;
  void WorkerScriptLoadFailed() override;
  void SelectAppCacheID(long long) override;
  blink::WebNotificationPresenter* NotificationPresenter() override;
  std::unique_ptr<blink::WebApplicationCacheHost> CreateApplicationCacheHost(
      blink::WebApplicationCacheHostClient*) override;
  std::unique_ptr<blink::WebServiceWorkerNetworkProvider>
  CreateServiceWorkerNetworkProvider() override;
  void SendDevToolsMessage(int session_id,
                           int call_id,
                           const blink::WebString& message,
                           const blink::WebString& state) override;
  blink::WebDevToolsAgentClient::WebKitClientMessageLoop*
  CreateDevToolsMessageLoop() override;
  std::unique_ptr<blink::WebWorkerFetchContext> CreateWorkerFetchContext(
      blink::WebServiceWorkerNetworkProvider*) override;

 private:
  void Shutdown();

  // WebSharedWorker will own |channel|.
  void ConnectToChannel(int connection_request_id,
                        blink::MessagePortChannel channel);

  // mojom::SharedWorker methods:
  void Connect(int connection_request_id,
               mojo::ScopedMessagePipeHandle port) override;
  void Terminate() override;

  mojo::Binding<mojom::SharedWorker> binding_;
  mojom::SharedWorkerHostPtr host_;
  const int route_id_;
  const std::string name_;
  bool running_ = false;
  GURL url_;
  blink::WebSharedWorker* impl_ = nullptr;
  std::unique_ptr<SharedWorkerDevToolsAgent> worker_devtools_agent_;

  using PendingChannel =
      std::pair<int /* connection_request_id */, blink::MessagePortChannel>;
  std::vector<PendingChannel> pending_channels_;

  ScopedChildProcessReference process_ref_;
  WebApplicationCacheHostImpl* app_cache_host_ = nullptr;  // Not owned.
  DISALLOW_COPY_AND_ASSIGN(EmbeddedSharedWorkerStub);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SHARED_WORKER_EMBEDDED_SHARED_WORKER_STUB_H_
