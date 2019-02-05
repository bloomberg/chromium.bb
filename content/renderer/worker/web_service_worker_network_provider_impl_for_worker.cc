// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/worker/web_service_worker_network_provider_impl_for_worker.h"

#include <utility>

#include "base/feature_list.h"
#include "content/public/common/origin_util.h"
#include "content/renderer/loader/navigation_response_override_parameters.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/loader/web_url_loader_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/service_worker/service_worker_provider_context.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"

namespace content {

std::unique_ptr<WebServiceWorkerNetworkProviderImplForWorker>
WebServiceWorkerNetworkProviderImplForWorker::Create(
    blink::mojom::ServiceWorkerProviderInfoForWorkerPtr info,
    network::mojom::URLLoaderFactoryAssociatedPtrInfo
        script_loader_factory_info,
    blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
    scoped_refptr<network::SharedURLLoaderFactory> fallback_loader_factory,
    bool is_secure_context,
    std::unique_ptr<NavigationResponseOverrideParameters> response_override) {
  auto provider =
      base::WrapUnique(new WebServiceWorkerNetworkProviderImplForWorker(
          is_secure_context, std::move(response_override)));
  if (blink::ServiceWorkerUtils::IsServicificationEnabled()) {
    DCHECK(info);
    provider->context_ = base::MakeRefCounted<ServiceWorkerProviderContext>(
        info->provider_id,
        blink::mojom::ServiceWorkerProviderType::kForSharedWorker,
        std::move(info->client_request), std::move(info->host_ptr_info),
        std::move(controller_info), std::move(fallback_loader_factory));
    if (script_loader_factory_info.is_valid()) {
      provider->script_loader_factory_.Bind(
          std::move(script_loader_factory_info));
    }
  } else {
    DCHECK(!info);
    int provider_id = ServiceWorkerProviderContext::GetNextId();
    auto host_info = blink::mojom::ServiceWorkerProviderHostInfo::New(
        provider_id, MSG_ROUTING_NONE,
        blink::mojom::ServiceWorkerProviderType::kForSharedWorker,
        true /* is_parent_frame_secure */, nullptr /* host_request */,
        nullptr /* client_ptr_info */);
    blink::mojom::ServiceWorkerContainerAssociatedRequest client_request =
        mojo::MakeRequest(&host_info->client_ptr_info);
    blink::mojom::ServiceWorkerContainerHostAssociatedPtrInfo host_ptr_info;
    host_info->host_request = mojo::MakeRequest(&host_ptr_info);

    provider->context_ = base::MakeRefCounted<ServiceWorkerProviderContext>(
        provider_id, blink::mojom::ServiceWorkerProviderType::kForSharedWorker,
        std::move(client_request), std::move(host_ptr_info),
        std::move(controller_info), std::move(fallback_loader_factory));
    if (ChildThreadImpl::current()) {
      ChildThreadImpl::current()->channel()->GetRemoteAssociatedInterface(
          &provider->dispatcher_host_);
      provider->dispatcher_host_->OnProviderCreated(std::move(host_info));
    } else {
      // current() may be null in tests. Silently drop messages sent over
      // ServiceWorkerContainerHost since we couldn't set it up correctly due
      // to this test limitation. This way we don't crash when the associated
      // interface ptr is used.
      //
      // TODO(falken): Just give ServiceWorkerProviderContext a null interface
      // ptr and make the callsites deal with it. They are supposed to anyway
      // because OnNetworkProviderDestroyed() can reset the ptr to null at any
      // time.
      mojo::AssociateWithDisconnectedPipe(host_info->host_request.PassHandle());
    }
  }
  return provider;
}

WebServiceWorkerNetworkProviderImplForWorker::
    ~WebServiceWorkerNetworkProviderImplForWorker() {
  if (context())
    context()->OnNetworkProviderDestroyed();
}

void WebServiceWorkerNetworkProviderImplForWorker::WillSendRequest(
    blink::WebURLRequest& request) {
  DCHECK_EQ(blink::mojom::RequestContextType::SHARED_WORKER,
            request.GetRequestContext());
  auto extra_data = std::make_unique<RequestExtraData>();
  extra_data->set_service_worker_provider_id(provider_id());
  extra_data->set_initiated_in_secure_context(is_secure_context_);
  if (response_override_) {
    DCHECK(base::FeatureList::IsEnabled(network::features::kNetworkService));
    extra_data->set_navigation_response_override(std::move(response_override_));
  }
  request.SetExtraData(std::move(extra_data));
}

blink::mojom::ControllerServiceWorkerMode
WebServiceWorkerNetworkProviderImplForWorker::IsControlledByServiceWorker() {
  if (!context())
    return blink::mojom::ControllerServiceWorkerMode::kNoController;
  return context()->IsControlledByServiceWorker();
}

int64_t
WebServiceWorkerNetworkProviderImplForWorker::ControllerServiceWorkerID() {
  if (!context())
    return blink::mojom::kInvalidServiceWorkerVersionId;
  return context()->GetControllerVersionId();
}

std::unique_ptr<blink::WebURLLoader>
WebServiceWorkerNetworkProviderImplForWorker::CreateURLLoader(
    const blink::WebURLRequest& request,
    std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
        task_runner_handle) {
  // We only get here for the main script request from the shadow page.
  // importScripts() and other subresource fetches are handled on the worker
  // thread by WebWorkerFetchContextImpl.
  DCHECK_EQ(blink::mojom::RequestContextType::SHARED_WORKER,
            request.GetRequestContext());

  // S13nServiceWorker:
  // We only install our own URLLoader if Servicification is enabled.
  if (!blink::ServiceWorkerUtils::IsServicificationEnabled())
    return nullptr;

  // If the |script_loader_factory_| exists, use it.
  if (script_loader_factory_) {
    RenderThreadImpl* render_thread = RenderThreadImpl::current();
    if (!render_thread) {
      // RenderThreadImpl is nullptr in some tests.
      return nullptr;
    }

    // TODO(crbug.com/796425): Temporarily wrap the raw
    // mojom::URLLoaderFactory pointer into SharedURLLoaderFactory.
    return std::make_unique<WebURLLoaderImpl>(
        render_thread->resource_dispatcher(), std::move(task_runner_handle),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            script_loader_factory_.get()));
  }

  // Otherwise go to default resource loading.
  return nullptr;
}

int WebServiceWorkerNetworkProviderImplForWorker::provider_id() const {
  if (!context_)
    return kInvalidServiceWorkerProviderId;
  return context_->provider_id();
}

WebServiceWorkerNetworkProviderImplForWorker::
    WebServiceWorkerNetworkProviderImplForWorker(
        bool is_secure_context,
        std::unique_ptr<NavigationResponseOverrideParameters> response_override)
    : is_secure_context_(is_secure_context),
      response_override_(std::move(response_override)) {}

}  // namespace content
