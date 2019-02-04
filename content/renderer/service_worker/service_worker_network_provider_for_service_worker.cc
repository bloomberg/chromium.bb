// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_network_provider_for_service_worker.h"

#include <utility>

#include "content/public/common/resource_type.h"
#include "content/public/renderer/url_loader_throttle_provider.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/loader/web_url_loader_impl.h"
#include "content/renderer/loader/web_url_request_util.h"
#include "content/renderer/render_thread_impl.h"
#include "ipc/ipc_message.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"

namespace content {

ServiceWorkerNetworkProviderForServiceWorker::
    ServiceWorkerNetworkProviderForServiceWorker(
        int provider_id,
        network::mojom::URLLoaderFactoryAssociatedPtrInfo
            script_loader_factory_info)
    : provider_id_(provider_id),
      script_loader_factory_(std::move(script_loader_factory_info)) {}

ServiceWorkerNetworkProviderForServiceWorker::
    ~ServiceWorkerNetworkProviderForServiceWorker() = default;

void ServiceWorkerNetworkProviderForServiceWorker::WillSendRequest(
    blink::WebURLRequest& request) {
  ResourceType resource_type = WebURLRequestToResourceType(request);
  DCHECK_EQ(resource_type, ResourceType::RESOURCE_TYPE_SERVICE_WORKER);

  auto extra_data = std::make_unique<RequestExtraData>();
  extra_data->set_service_worker_provider_id(provider_id_);
  extra_data->set_originated_from_service_worker(true);
  // Service workers are only available in secure contexts, so all requests
  // are initiated in a secure context.
  extra_data->set_initiated_in_secure_context(true);

  // The RenderThreadImpl or its URLLoaderThrottleProvider member may not be
  // valid in some tests.
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  if (render_thread && render_thread->url_loader_throttle_provider()) {
    extra_data->set_url_loader_throttles(
        render_thread->url_loader_throttle_provider()->CreateThrottles(
            MSG_ROUTING_NONE, request, resource_type));
  }

  request.SetExtraData(std::move(extra_data));
}

std::unique_ptr<blink::WebURLLoader>
ServiceWorkerNetworkProviderForServiceWorker::CreateURLLoader(
    const blink::WebURLRequest& request,
    std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
        task_runner_handle) {
  // We only get here for the main script request from the shadow page.
  // importScripts() and other subresource fetches are handled on the worker
  // thread by ServiceWorkerFetchContextImpl.
  DCHECK_EQ(blink::mojom::RequestContextType::SERVICE_WORKER,
            request.GetRequestContext());

  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  // RenderThreadImpl may be null in some tests.
  if (render_thread && script_loader_factory() &&
      blink::ServiceWorkerUtils::IsServicificationEnabled()) {
    // TODO(crbug.com/796425): Temporarily wrap the raw
    // mojom::URLLoaderFactory pointer into SharedURLLoaderFactory.
    return std::make_unique<WebURLLoaderImpl>(
        render_thread->resource_dispatcher(), std::move(task_runner_handle),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            script_loader_factory()));
  }
  return nullptr;
}

}  // namespace content
