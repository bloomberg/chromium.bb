// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_DEDICATED_WORKER_HOST_H_
#define CONTENT_BROWSER_WORKER_HOST_DEDICATED_WORKER_HOST_H_

#include "content/browser/browser_interface_broker_impl.h"
#include "content/public/browser/render_process_host.h"

#include "mojo/public/cpp/bindings/receiver.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "third_party/blink/public/mojom/idle/idle_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom-forward.h"
#include "third_party/blink/public/mojom/websockets/websocket_connector.mojom-forward.h"
#include "third_party/blink/public/mojom/worker/dedicated_worker_host_factory.mojom.h"

namespace url {
class Origin;
}

namespace content {

class AppCacheNavigationHandle;
class ServiceWorkerNavigationHandle;
class ServiceWorkerObjectHost;

// Creates a host factory for a dedicated worker. This must be called on the UI
// thread.
void CreateDedicatedWorkerHostFactory(
    int process_id,
    int parent_render_frame_id,
    const url::Origin& origin,
    blink::mojom::DedicatedWorkerHostFactoryRequest request);

// A host for a single dedicated worker. Its lifetime is managed by the
// DedicatedWorkerGlobalScope of the corresponding worker in the renderer via a
// StrongBinding. This lives on the UI thread.
class DedicatedWorkerHost : public service_manager::mojom::InterfaceProvider {
 public:
  DedicatedWorkerHost(int worker_process_id,
                      int ancestor_render_frame_id,
                      const url::Origin& origin);
  ~DedicatedWorkerHost() final;

  void BindBrowserInterfaceBrokerReceiver(
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> receiver);

  // May return nullptr.
  RenderProcessHost* GetProcessHost() {
    return RenderProcessHost::FromID(worker_process_id_);
  }
  const url::Origin& GetOrigin() { return origin_; }

  // service_manager::mojom::InterfaceProvider:
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle interface_pipe) override;

  // PlzDedicatedWorker:
  void StartScriptLoad(
      const GURL& script_url,
      const url::Origin& request_initiator_origin,
      network::mojom::CredentialsMode credentials_mode,
      blink::mojom::FetchClientSettingsObjectPtr
          outside_fetch_client_settings_object,
      blink::mojom::BlobURLTokenPtr blob_url_token,
      blink::mojom::DedicatedWorkerHostFactoryClientPtr client);

 private:
  void RegisterMojoInterfaces();

  // Called from WorkerScriptFetchInitiator. Continues starting the dedicated
  // worker in the renderer process.
  //
  // |main_script_load_params| is sent to the renderer process and to be used to
  // load the dedicated worker main script pre-requested by the browser process.
  //
  // |subresource_loader_factories| is sent to the renderer process and is to be
  // used to request subresources where applicable. For example, this allows the
  // dedicated worker to load chrome-extension:// URLs which the renderer's
  // default loader factory can't load.
  //
  // NetworkService (PlzWorker):
  // |controller| contains information about the service worker controller. Once
  // a ServiceWorker object about the controller is prepared, it is registered
  // to |controller_service_worker_object_host|.
  void DidStartScriptLoad(
      std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
          subresource_loader_factories,
      blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
      blink::mojom::ControllerServiceWorkerInfoPtr controller,
      base::WeakPtr<ServiceWorkerObjectHost>
          controller_service_worker_object_host,
      bool success);

  void CreateNetworkFactory(network::mojom::URLLoaderFactoryRequest request,
                            RenderFrameHostImpl* render_frame_host);

  void CreateWebUsbService(blink::mojom::WebUsbServiceRequest request);

  void CreateWebSocketConnector(
      blink::mojom::WebSocketConnectorRequest request);

  void CreateDedicatedWorker(
      blink::mojom::DedicatedWorkerHostFactoryRequest request);

  void CreateIdleManager(blink::mojom::IdleManagerRequest request);

  // May return a nullptr.
  RenderFrameHostImpl* GetAncestorRenderFrameHost();
  // The ID of the render process host that hosts this worker.
  const int worker_process_id_;

  // The ID of the frame that owns this worker, either directly, or (in the case
  // of nested workers) indirectly via a tree of dedicated workers.
  const int ancestor_render_frame_id_;

  const url::Origin origin_;

  // This is kept alive during the lifetime of the dedicated worker, since it's
  // associated with Mojo interfaces (ServiceWorkerContainer and
  // URLLoaderFactory) that are needed to stay alive while the worker is
  // starting or running.
  blink::mojom::DedicatedWorkerHostFactoryClientPtr client_;

  std::unique_ptr<AppCacheNavigationHandle> appcache_handle_;
  std::unique_ptr<ServiceWorkerNavigationHandle> service_worker_handle_;

  service_manager::BinderRegistry registry_;

  BrowserInterfaceBrokerImpl<DedicatedWorkerHost, const url::Origin&> broker_{
      this};
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker> broker_receiver_{
      &broker_};

  base::WeakPtrFactory<DedicatedWorkerHost> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DedicatedWorkerHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_DEDICATED_WORKER_HOST_H_
