// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "content/browser/worker_host/dedicated_worker_host.h"

#include "base/bind.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/interface_provider_filtering.h"
#include "content/browser/renderer_interface_binders.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/websockets/websocket_manager.h"
#include "content/browser/worker_host/worker_script_fetch_initiator.h"
#include "content/common/navigation_subresource_loader_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom.h"
#include "url/origin.h"

namespace content {
namespace {

// A host for a single dedicated worker. Its lifetime is managed by the
// DedicatedWorkerGlobalScope of the corresponding worker in the renderer via a
// StrongBinding. This lives on the UI thread.
class DedicatedWorkerHost : public service_manager::mojom::InterfaceProvider {
 public:
  DedicatedWorkerHost(int process_id,
                      int ancestor_render_frame_id,
                      const url::Origin& origin)
      : process_id_(process_id),
        ancestor_render_frame_id_(ancestor_render_frame_id),
        origin_(origin),
        weak_factory_(this) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    RegisterMojoInterfaces();
  }

  // service_manager::mojom::InterfaceProvider:
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle interface_pipe) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    RenderProcessHost* process = RenderProcessHost::FromID(process_id_);
    if (!process)
      return;

    // See if the registry that is specific to this worker host wants to handle
    // the interface request.
    if (registry_.TryBindInterface(interface_name, &interface_pipe))
      return;

    BindWorkerInterface(interface_name, std::move(interface_pipe), process,
                        origin_);
  }

  // PlzDedicatedWorker:
  void LoadDedicatedWorker(
      const GURL& script_url,
      const url::Origin& request_initiator_origin,
      blink::mojom::BlobURLTokenPtr blob_url_token,
      blink::mojom::DedicatedWorkerHostFactoryClientPtr client) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(blink::features::IsPlzDedicatedWorkerEnabled());

    auto* render_process_host = RenderProcessHost::FromID(process_id_);
    if (!render_process_host) {
      client->OnScriptLoadFailed();
      return;
    }
    auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
        render_process_host->GetStoragePartition());

    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory;
    if (blob_url_token) {
      if (!script_url.SchemeIsBlob()) {
        mojo::ReportBadMessage("DWH_NOT_BLOB_URL");
        return;
      }
      blob_url_loader_factory =
          ChromeBlobStorageContext::URLLoaderFactoryForToken(
              storage_partition_impl->browser_context(),
              std::move(blob_url_token));
    }

    appcache_handle_ = std::make_unique<AppCacheNavigationHandle>(
        storage_partition_impl->GetAppCacheService(), process_id_);

    WorkerScriptFetchInitiator::Start(
        process_id_, script_url, request_initiator_origin, RESOURCE_TYPE_WORKER,
        storage_partition_impl->GetServiceWorkerContext(),
        appcache_handle_->core(), std::move(blob_url_loader_factory),
        storage_partition_impl,
        base::BindOnce(&DedicatedWorkerHost::DidLoadDedicatedWorker,
                       weak_factory_.GetWeakPtr(), std::move(client)));
  }

 private:
  void RegisterMojoInterfaces() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    registry_.AddInterface(base::BindRepeating(
        &DedicatedWorkerHost::CreateWebSocket, base::Unretained(this)));
    registry_.AddInterface(base::BindRepeating(
        &DedicatedWorkerHost::CreateWebUsbService, base::Unretained(this)));
    registry_.AddInterface(base::BindRepeating(
        &DedicatedWorkerHost::CreateDedicatedWorker, base::Unretained(this)));
  }

  void DidLoadDedicatedWorker(
      blink::mojom::DedicatedWorkerHostFactoryClientPtr client,
      blink::mojom::ServiceWorkerProviderInfoForWorkerPtr
          service_worker_provider_info,
      network::mojom::URLLoaderFactoryAssociatedPtrInfo
          main_script_loader_factory,
      std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
          subresource_loader_factories,
      blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
      base::Optional<SubresourceLoaderParams> subresource_loader_params,
      bool success) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(blink::features::IsPlzDedicatedWorkerEnabled());
    // |main_script_loader_factory| is not used when NetworkService is enabled.
    DCHECK(!main_script_loader_factory);

    if (!success) {
      client->OnScriptLoadFailed();
      return;
    }

    auto* render_process_host = RenderProcessHost::FromID(process_id_);
    if (!render_process_host) {
      client->OnScriptLoadFailed();
      return;
    }
    auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
        render_process_host->GetStoragePartition());

    // Set up the default network loader factory.
    network::mojom::URLLoaderFactoryPtrInfo default_factory_info;
    CreateNetworkFactory(mojo::MakeRequest(&default_factory_info),
                         storage_partition_impl->GetNetworkContext());
    subresource_loader_factories->default_factory_info() =
        std::move(default_factory_info);

    // Prepare the controller service worker info to pass to the renderer.
    blink::mojom::ControllerServiceWorkerInfoPtr controller;
    blink::mojom::ServiceWorkerObjectAssociatedPtrInfo
        service_worker_remote_object;
    blink::mojom::ServiceWorkerState service_worker_state;
    if (subresource_loader_params &&
        subresource_loader_params->controller_service_worker_info) {
      controller =
          std::move(subresource_loader_params->controller_service_worker_info);
      // |object_info| can be nullptr when the service worker context or the
      // service worker version is gone during dedicated worker startup.
      if (controller->object_info) {
        controller->object_info->request =
            mojo::MakeRequest(&service_worker_remote_object);
        service_worker_state = controller->object_info->state;
      }
    }

    client->OnScriptLoaded(std::move(service_worker_provider_info),
                           std::move(main_script_load_params),
                           std::move(subresource_loader_factories),
                           std::move(controller));

    // |service_worker_remote_object| is an associated interface ptr, so calls
    // can't be made on it until its request endpoint is sent. Now that the
    // request endpoint was sent, it can be used, so add it to
    // ServiceWorkerObjectHost.
    if (service_worker_remote_object.is_valid()) {
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::IO},
          base::BindOnce(
              &ServiceWorkerObjectHost::AddRemoteObjectPtrAndUpdateState,
              subresource_loader_params->controller_service_worker_object_host,
              std::move(service_worker_remote_object), service_worker_state));
    }
  }

  // This is similar to
  // RenderFrameHostImpl::CreateNetworkServiceDefaultFactoryAndObserve, but this
  // host doesn't observe network service crashes. Instead, the renderer detects
  // the connection error and terminates the worker.
  // TODO(nhiroki): Implement this mechanism. See EmbeddedSharedWorkerStub's
  // |default_factory_connection_error_handler_holder_| for reference.
  // (https://crbug.com/906991)
  void CreateNetworkFactory(network::mojom::URLLoaderFactoryRequest request,
                            network::mojom::NetworkContext* network_context) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    network::mojom::URLLoaderFactoryParamsPtr params =
        network::mojom::URLLoaderFactoryParams::New();
    params->process_id = process_id_;
    // TODO(lukasza): https://crbug.com/792546: Start using CORB.
    params->is_corb_enabled = false;

    network_context->CreateURLLoaderFactory(std::move(request),
                                            std::move(params));
  }

  void CreateWebUsbService(blink::mojom::WebUsbServiceRequest request) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    auto* host =
        RenderFrameHostImpl::FromID(process_id_, ancestor_render_frame_id_);
    GetContentClient()->browser()->CreateWebUsbService(host,
                                                       std::move(request));
  }

  void CreateWebSocket(network::mojom::WebSocketRequest request) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    network::mojom::AuthenticationHandlerPtr auth_handler;
    auto* frame =
        RenderFrameHost::FromID(process_id_, ancestor_render_frame_id_);
    if (!frame) {
      // In some cases |frame| can be null. In such cases the worker will
      // soon be terminated too, so let's abort the connection.
      request.ResetWithReason(network::mojom::WebSocket::kInsufficientResources,
                              "The parent frame has already been gone.");
      return;
    }

    GetContentClient()->browser()->WillCreateWebSocket(frame, &request,
                                                       &auth_handler);

    WebSocketManager::CreateWebSocket(process_id_, ancestor_render_frame_id_,
                                      origin_, std::move(auth_handler),
                                      std::move(request));
  }

  void CreateDedicatedWorker(
      blink::mojom::DedicatedWorkerHostFactoryRequest request) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    CreateDedicatedWorkerHostFactory(process_id_, ancestor_render_frame_id_,
                                     origin_, std::move(request));
  }

  const int process_id_;
  // ancestor_render_frame_id_ is the id of the frame that owns this worker,
  // either directly, or (in the case of nested workers) indirectly via a tree
  // of dedicated workers.
  const int ancestor_render_frame_id_;
  const url::Origin origin_;

  std::unique_ptr<AppCacheNavigationHandle> appcache_handle_;

  service_manager::BinderRegistry registry_;

  base::WeakPtrFactory<DedicatedWorkerHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DedicatedWorkerHost);
};

// A factory for creating DedicatedWorkerHosts. Its lifetime is managed by
// the renderer over mojo via a StrongBinding. This lives on the UI thread.
class DedicatedWorkerHostFactoryImpl
    : public blink::mojom::DedicatedWorkerHostFactory {
 public:
  DedicatedWorkerHostFactoryImpl(int process_id,
                                 int ancestor_render_frame_id,
                                 const url::Origin& parent_context_origin)
      : process_id_(process_id),
        ancestor_render_frame_id_(ancestor_render_frame_id),
        parent_context_origin_(parent_context_origin) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
  }

  // blink::mojom::DedicatedWorkerHostFactory:
  void CreateDedicatedWorker(
      const url::Origin& origin,
      service_manager::mojom::InterfaceProviderRequest request) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (blink::features::IsPlzDedicatedWorkerEnabled()) {
      mojo::ReportBadMessage("DWH_INVALID_WORKER_CREATION");
      return;
    }

    // TODO(crbug.com/729021): Once |parent_context_origin_| no longer races
    // with the request for |DedicatedWorkerHostFactory|, enforce that
    // the worker's origin either matches the origin of the creating context
    // (Document or DedicatedWorkerGlobalScope), or is unique.
    mojo::MakeStrongBinding(std::make_unique<DedicatedWorkerHost>(
                                process_id_, ancestor_render_frame_id_, origin),
                            FilterRendererExposedInterfaces(
                                blink::mojom::kNavigation_DedicatedWorkerSpec,
                                process_id_, std::move(request)));
  }

  // PlzDedicatedWorker:
  void CreateAndStartLoad(
      const GURL& script_url,
      const url::Origin& request_initiator_origin,
      blink::mojom::BlobURLTokenPtr blob_url_token,
      blink::mojom::DedicatedWorkerHostFactoryClientPtr client) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!blink::features::IsPlzDedicatedWorkerEnabled()) {
      mojo::ReportBadMessage("DWH_BROWSER_SCRIPT_FETCH_DISABLED");
      return;
    }

    // TODO(crbug.com/729021): Once |parent_context_origin_| no longer races
    // with the request for |DedicatedWorkerHostFactory|, enforce that
    // the worker's origin either matches the origin of the creating context
    // (Document or DedicatedWorkerGlobalScope), or is unique.
    auto host = std::make_unique<DedicatedWorkerHost>(
        process_id_, ancestor_render_frame_id_, request_initiator_origin);
    auto* host_raw = host.get();
    service_manager::mojom::InterfaceProviderPtr interface_provider;
    mojo::MakeStrongBinding(
        std::move(host),
        FilterRendererExposedInterfaces(
            blink::mojom::kNavigation_DedicatedWorkerSpec, process_id_,
            mojo::MakeRequest(&interface_provider)));
    client->OnWorkerHostCreated(std::move(interface_provider));
    host_raw->LoadDedicatedWorker(script_url, request_initiator_origin,
                                  std::move(blob_url_token), std::move(client));
  }

 private:
  const int process_id_;
  const int ancestor_render_frame_id_;
  const url::Origin parent_context_origin_;

  DISALLOW_COPY_AND_ASSIGN(DedicatedWorkerHostFactoryImpl);
};

}  // namespace

void CreateDedicatedWorkerHostFactory(
    int process_id,
    int ancestor_render_frame_id,
    const url::Origin& origin,
    blink::mojom::DedicatedWorkerHostFactoryRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  mojo::MakeStrongBinding(std::make_unique<DedicatedWorkerHostFactoryImpl>(
                              process_id, ancestor_render_frame_id, origin),
                          std::move(request));
}

}  // namespace content
