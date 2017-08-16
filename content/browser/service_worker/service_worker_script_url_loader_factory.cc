// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_script_url_loader_factory.h"

#include <memory>
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_script_url_loader.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/resource_response.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace content {

ServiceWorkerScriptURLLoaderFactory::ServiceWorkerScriptURLLoaderFactory(
    base::WeakPtr<ServiceWorkerContextCore> context,
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
    scoped_refptr<URLLoaderFactoryGetter> loader_factory_getter)
    : context_(context),
      provider_host_(provider_host),
      blob_storage_context_(blob_storage_context),
      loader_factory_getter_(loader_factory_getter) {}

ServiceWorkerScriptURLLoaderFactory::~ServiceWorkerScriptURLLoaderFactory() =
    default;

void ServiceWorkerScriptURLLoaderFactory::CreateLoaderAndStart(
    mojom::URLLoaderRequest request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& resource_request,
    mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (!ShouldHandleScriptRequest(resource_request)) {
    // If the request should not be handled by ServiceWorkerScriptURLLoader,
    // just fallback to the network. This needs a relaying as we use different
    // associated message pipes.
    // TODO(kinuko): Record the reason like what we do with netlog in
    // ServiceWorkerContextRequestHandler.
    (*loader_factory_getter_->GetNetworkFactory())
        ->CreateLoaderAndStart(std::move(request), routing_id, request_id,
                               options, resource_request, std::move(client),
                               traffic_annotation);
    return;
  }
  mojo::MakeStrongBinding(
      base::MakeUnique<ServiceWorkerScriptURLLoader>(
          routing_id, request_id, options, resource_request, std::move(client),
          context_, provider_host_, blob_storage_context_,
          loader_factory_getter_, traffic_annotation),
      std::move(request));
}

void ServiceWorkerScriptURLLoaderFactory::Clone(
    mojom::URLLoaderFactoryRequest request) {
  // This method is required to support synchronous requests which are not
  // performed during installation.
  NOTREACHED();
}

bool ServiceWorkerScriptURLLoaderFactory::ShouldHandleScriptRequest(
    const ResourceRequest& resource_request) {
  if (!context_ || !provider_host_)
    return false;

  // We only use the script cache for main script loading and
  // importScripts(), even if a cached script is xhr'd, we don't
  // retrieve it from the script cache.
  if (resource_request.resource_type != RESOURCE_TYPE_SERVICE_WORKER &&
      resource_request.resource_type != RESOURCE_TYPE_SCRIPT) {
    // TODO: Record bad message, we shouldn't come here for other
    // request types.
    return false;
  }

  scoped_refptr<ServiceWorkerVersion> version =
      provider_host_->running_hosted_version();

  // This could happen if browser-side has set the status to redundant but
  // the worker has not yet stopped. The worker is already doomed so just
  // reject the request. Handle it specially here because otherwise it'd be
  // unclear whether "REDUNDANT" should count as installed or not installed
  // when making decisions about how to handle the request and logging UMA.
  if (!version || version->status() == ServiceWorkerVersion::REDUNDANT)
    return false;

  // TODO: Make sure we don't handle the redirected request.

  // If script streaming is enabled, for installed service workers, typically
  // all the scripts are served via script streaming, so we don't come here.
  // However, we still come here when the service worker is A) importing a
  // script that was never installed, or B) loading the same script twice.
  // For now, return false here to fallback to network. Eventually, A) should
  // be deprecated (https://crbug.com/719052), and B) should be handled by
  // script streaming as well, see the TODO in
  // WebServiceWorkerInstalledScriptsManagerImpl::GetRawScriptData().
  //
  // When script streaming is not enabled, we get here even for the main
  // script. Therefore, ServiceWorkerScriptURLLoader must handle the request
  // (even though it currently just does a network fetch for now), because it
  // sets the main script's HTTP Response Info (via
  // ServiceWorkerVersion::SetMainScriptHttpResponseInfo()) which otherwise
  // would never be set.
  if (ServiceWorkerVersion::IsInstalled(version->status()) &&
      ServiceWorkerUtils::IsScriptStreamingEnabled()) {
    return false;
  }

  // TODO: Make sure we come here only for new / unknown scripts
  // once script streaming manager in the renderer side stops sending
  // resource requests for the known script URLs, i.e. add DCHECK for
  // version->script_cache_map()->LookupResourceId(url) ==
  // kInvalidServiceWorkerResourceId.
  //
  // Currently this could be false for the installing worker that imports
  // the same script twice (e.g. importScripts('dupe.js');
  // importScripts('dupe.js');).

  // Request should be served by ServiceWorkerScriptURLLoader.
  return true;
}

}  // namespace content
