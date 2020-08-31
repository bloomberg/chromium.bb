// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_DEDICATED_WORKER_HOST_H_
#define CONTENT_BROWSER_WORKER_HOST_DEDICATED_WORKER_HOST_H_

#include <memory>

#include "base/optional.h"
#include "base/scoped_observer.h"
#include "build/build_config.h"
#include "content/browser/browser_interface_broker_impl.h"
#include "content/public/browser/dedicated_worker_id.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "media/mojo/mojom/video_decode_perf_history.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom-forward.h"
#include "third_party/blink/public/mojom/idle/idle_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom-forward.h"
#include "third_party/blink/public/mojom/sms/sms_receiver.mojom-forward.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom-forward.h"
#include "third_party/blink/public/mojom/wake_lock/wake_lock.mojom-forward.h"
#include "third_party/blink/public/mojom/websockets/websocket_connector.mojom-forward.h"
#include "third_party/blink/public/mojom/webtransport/quic_transport_connector.mojom-forward.h"
#include "third_party/blink/public/mojom/worker/dedicated_worker_host.mojom.h"
#include "third_party/blink/public/mojom/worker/dedicated_worker_host_factory.mojom.h"
#include "third_party/blink/public/mojom/worker/subresource_loader_updater.mojom.h"

#if !defined(OS_ANDROID)
#include "third_party/blink/public/mojom/serial/serial.mojom-forward.h"
#endif

namespace url {
class Origin;
}

namespace content {

class CrossOriginEmbedderPolicyReporter;
class DedicatedWorkerServiceImpl;
class ServiceWorkerMainResourceHandle;
class ServiceWorkerObjectHost;
class StoragePartitionImpl;

// Creates a host factory for a dedicated worker. This must be called on the UI
// thread.
CONTENT_EXPORT void CreateDedicatedWorkerHostFactory(
    int worker_process_id,
    base::Optional<GlobalFrameRoutingId> creator_render_frame_host_id,
    GlobalFrameRoutingId ancestor_render_frame_host_id,
    const url::Origin& creator_origin,
    const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    mojo::PendingReceiver<blink::mojom::DedicatedWorkerHostFactory> receiver);

// A host for a single dedicated worker. It deletes itself upon Mojo
// disconnection from the worker in the renderer or when the RenderProcessHost
// of the worker is destroyed. This lives on the UI thread.
class DedicatedWorkerHost final : public blink::mojom::DedicatedWorkerHost,
                                  public RenderProcessHostObserver {
 public:
  DedicatedWorkerHost(
      DedicatedWorkerServiceImpl* service,
      DedicatedWorkerId id,
      RenderProcessHost* worker_process_host,
      base::Optional<GlobalFrameRoutingId> creator_render_frame_host_id,
      GlobalFrameRoutingId ancestor_render_frame_host_id,
      const url::Origin& creator_origin,
      const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
          coep_reporter,
      mojo::PendingReceiver<blink::mojom::DedicatedWorkerHost> host);
  ~DedicatedWorkerHost() final;

  void BindBrowserInterfaceBrokerReceiver(
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> receiver);

  RenderProcessHost* GetProcessHost() { return worker_process_host_; }
  const url::Origin& GetWorkerOrigin() { return worker_origin_; }

  void CreateIdleManager(
      mojo::PendingReceiver<blink::mojom::IdleManager> receiver);
  void CreateNestedDedicatedWorker(
      mojo::PendingReceiver<blink::mojom::DedicatedWorkerHostFactory> receiver);
  void BindSmsReceiverReceiver(
      mojo::PendingReceiver<blink::mojom::SmsReceiver> receiver);
  void CreateWebUsbService(
      mojo::PendingReceiver<blink::mojom::WebUsbService> receiver);
  void CreateWebSocketConnector(
      mojo::PendingReceiver<blink::mojom::WebSocketConnector> receiver);
  void CreateQuicTransportConnector(
      mojo::PendingReceiver<blink::mojom::QuicTransportConnector> receiver);
  void CreateWakeLockService(
      mojo::PendingReceiver<blink::mojom::WakeLockService> receiver);
  void BindCacheStorage(
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver);

#if !defined(OS_ANDROID)
  void BindSerialService(
      mojo::PendingReceiver<blink::mojom::SerialService> receiver);
#endif

  // blink::mojom::DedicatedWorkerHost:
  void LifecycleStateChanged(blink::mojom::FrameLifecycleState state) override;

  // TODO(dtapuska): This state needs to be hooked up to the
  // ServiceWorkerProviderHost so the correct state is queried when looking
  // for frozen dedicated workers. crbug.com/968417
  bool is_frozen() const { return is_frozen_; }

  // PlzDedicatedWorker:
  void StartScriptLoad(
      const GURL& script_url,
      network::mojom::CredentialsMode credentials_mode,
      blink::mojom::FetchClientSettingsObjectPtr
          outside_fetch_client_settings_object,
      mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token,
      mojo::Remote<blink::mojom::DedicatedWorkerHostFactoryClient> client);

  void ReportNoBinderForInterface(const std::string& error);

 private:
  // RenderProcessHostObserver:
  void RenderProcessExited(RenderProcessHost* render_process_host,
                           const ChildProcessTerminationInfo& info) override;

  // Called from WorkerScriptFetchInitiator. Continues starting the dedicated
  // worker in the renderer process.
  //
  // |success| is true only when the script fetch succeeded.
  //
  // Note: None of the following parameters are valid if |success| is false.
  //
  // |main_script_load_params| is sent to the renderer process and to be used to
  // load the dedicated worker main script pre-requested by the browser process.
  //
  // |subresource_loader_factories| is sent to the renderer process and is to be
  // used to request subresources where applicable. For example, this allows the
  // dedicated worker to load chrome-extension:// URLs which the renderer's
  // default loader factory can't load.
  //
  // |controller| contains information about the service worker controller. Once
  // a ServiceWorker object about the controller is prepared, it is registered
  // to |controller_service_worker_object_host|.
  //
  // |final_response_url| is the URL calculated from the initial request URL,
  // redirect chain, and URLs fetched via service worker.
  // https://fetch.spec.whatwg.org/#concept-response-url
  void DidStartScriptLoad(
      bool success,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          subresource_loader_factories,
      blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
      blink::mojom::ControllerServiceWorkerInfoPtr controller,
      base::WeakPtr<ServiceWorkerObjectHost>
          controller_service_worker_object_host,
      const GURL& final_response_url);

  // Sets up the observer of network service crash.
  void ObserveNetworkServiceCrash(StoragePartitionImpl* storage_partition_impl);

  // Creates a network factory for subresource requests from this worker. The
  // network factory is meant to be passed to the renderer.
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
  CreateNetworkFactoryForSubresources(
      RenderFrameHostImpl* ancestor_render_frame_host,
      bool* bypass_redirect_checks);

  // Updates subresource loader factories. This is supposed to be called when
  // out-of-process Network Service crashes.
  void UpdateSubresourceLoaderFactories();

  void OnMojoDisconnect();

  DedicatedWorkerServiceImpl* const service_;

  // An internal ID that is unique within a storage partition.
  const DedicatedWorkerId id_;

  // The RenderProcessHost that hosts this worker.
  RenderProcessHost* const worker_process_host_;

  ScopedObserver<RenderProcessHost, RenderProcessHostObserver>
      scoped_process_host_observer_;

  // The ID of the frame that directly starts this worker. This is base::nullopt
  // when this worker is nested.
  const base::Optional<GlobalFrameRoutingId> creator_render_frame_host_id_;

  // The ID of the frame that owns this worker, either directly, or (in the case
  // of nested workers) indirectly via a tree of dedicated workers.
  const GlobalFrameRoutingId ancestor_render_frame_host_id_;

  // The origin of the frame or dedicated worker that starts this worker.
  const url::Origin creator_origin_;

  // The origin of this worker.
  // https://html.spec.whatwg.org/C/#concept-settings-object-origin
  const url::Origin worker_origin_;

  // The DedicatedWorker's Cross-Origin-Embedder-Policy(COEP). It is equals to
  // the nearest ancestor frame host's COEP:
  // https://mikewest.github.io/corpp/#initialize-embedder-policy-for-global
  const network::CrossOriginEmbedderPolicy cross_origin_embedder_policy_;

  // This is kept alive during the lifetime of the dedicated worker, since it's
  // associated with Mojo interfaces (ServiceWorkerContainer and
  // URLLoaderFactory) that are needed to stay alive while the worker is
  // starting or running.
  mojo::Remote<blink::mojom::DedicatedWorkerHostFactoryClient> client_;

  std::unique_ptr<ServiceWorkerMainResourceHandle> service_worker_handle_;

  BrowserInterfaceBrokerImpl<DedicatedWorkerHost, const url::Origin&> broker_{
      this};
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker> broker_receiver_{
      &broker_};
  mojo::Receiver<blink::mojom::DedicatedWorkerHost> host_receiver_;

  // Indicates if subresource loaders of this worker support file URLs.
  bool file_url_support_ = false;

  // The liveness state of the dedicated worker in the renderer.
  bool is_frozen_ = false;

  // For observing Network Service connection errors only.
  mojo::Remote<network::mojom::URLLoaderFactory>
      network_service_connection_error_handler_holder_;
  mojo::Remote<blink::mojom::SubresourceLoaderUpdater>
      subresource_loader_updater_;

  // The endpoint of this mojo interface is the RenderFrameHostImpl's COEP
  // reporter. The COEP endpoint is correct, but the context_url is the
  // Document's URL.
  // TODO(arthursonzogni): After landing PlzDedicatedWorker, make the
  // DedicatedWorkerHost to have its own COEP reporter using the right
  // context_url.
  mojo::Remote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter_;  // Never null.

  base::WeakPtrFactory<DedicatedWorkerHost> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DedicatedWorkerHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_DEDICATED_WORKER_HOST_H_
