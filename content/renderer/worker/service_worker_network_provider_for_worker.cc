// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/worker/service_worker_network_provider_for_worker.h"

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
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"

namespace content {

std::unique_ptr<ServiceWorkerNetworkProviderForWorker>
ServiceWorkerNetworkProviderForWorker::Create(
    blink::mojom::ServiceWorkerProviderInfoForWorkerPtr info,
    network::mojom::URLLoaderFactoryAssociatedPtrInfo
        script_loader_factory_info,
    blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
    scoped_refptr<network::SharedURLLoaderFactory> fallback_loader_factory,
    bool is_secure_context,
    std::unique_ptr<NavigationResponseOverrideParameters> response_override) {
  auto provider = base::WrapUnique(new ServiceWorkerNetworkProviderForWorker(
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
      mojo::AssociateWithDisconnectedPipe(host_info->host_request.PassHandle());
    }
  }
  return provider;
}

ServiceWorkerNetworkProviderForWorker::
    ~ServiceWorkerNetworkProviderForWorker() {
  if (context())
    context()->OnNetworkProviderDestroyed();
}

void ServiceWorkerNetworkProviderForWorker::WillSendRequest(
    blink::WebURLRequest& request) {
  auto extra_data = std::make_unique<RequestExtraData>();
  extra_data->set_service_worker_provider_id(provider_id());
  extra_data->set_initiated_in_secure_context(is_secure_context_);
  if (response_override_) {
    DCHECK(base::FeatureList::IsEnabled(network::features::kNetworkService));
    extra_data->set_navigation_response_override(std::move(response_override_));
  }
  request.SetExtraData(std::move(extra_data));
}

std::unique_ptr<blink::WebURLLoader>
ServiceWorkerNetworkProviderForWorker::CreateURLLoader(
    const blink::WebURLRequest& request,
    std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
        task_runner_handle) {
  if (request.GetRequestContext() !=
      blink::mojom::RequestContextType::SHARED_WORKER) {
    // This provider is only used for requests from the shadow page, which is
    // created to load the shared worker's main script. But shadow pages
    // sometimes request strange things like CSS resources because consumers
    // think it's a real frame. Just return nullptr to use the default loader
    // instead of the script loader.
    return nullptr;
  }

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

blink::mojom::ControllerServiceWorkerMode
ServiceWorkerNetworkProviderForWorker::IsControlledByServiceWorker() {
  if (!context())
    return blink::mojom::ControllerServiceWorkerMode::kNoController;
  return context()->IsControlledByServiceWorker();
}

int64_t ServiceWorkerNetworkProviderForWorker::ControllerServiceWorkerID() {
  if (!context())
    return blink::mojom::kInvalidServiceWorkerVersionId;
  return context()->GetControllerVersionId();
}

void ServiceWorkerNetworkProviderForWorker::DispatchNetworkQuiet() {}

int ServiceWorkerNetworkProviderForWorker::provider_id() const {
  if (!context_)
    return kInvalidServiceWorkerProviderId;
  return context_->provider_id();
}

ServiceWorkerNetworkProviderForWorker::ServiceWorkerNetworkProviderForWorker(
    bool is_secure_context,
    std::unique_ptr<NavigationResponseOverrideParameters> response_override)
    : is_secure_context_(is_secure_context),
      response_override_(std::move(response_override)) {}

}  // namespace content
