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

namespace content {

ServiceWorkerScriptURLLoaderFactory::ServiceWorkerScriptURLLoaderFactory(
    base::WeakPtr<ServiceWorkerContextCore> context,
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    scoped_refptr<URLLoaderFactoryGetter> loader_factory_getter)
    : context_(context),
      provider_host_(provider_host),
      loader_factory_getter_(loader_factory_getter) {
  DCHECK(provider_host_->IsHostToRunningServiceWorker());
}

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
      std::make_unique<ServiceWorkerScriptURLLoader>(
          routing_id, request_id, options, resource_request, std::move(client),
          provider_host_->running_hosted_version(), loader_factory_getter_,
          traffic_annotation),
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

  scoped_refptr<ServiceWorkerVersion> version =
      provider_host_->running_hosted_version();
  if (!version)
    return false;

  // We use ServiceWorkerScriptURLLoader only for fetching the service worker
  // main script (RESOURCE_TYPE_SERVICE_WORKER) or importScripts()
  // (RESOURCE_TYPE_SCRIPT).
  switch (resource_request.resource_type) {
    case RESOURCE_TYPE_SERVICE_WORKER:
      // The main script should be fetched only when we start a new service
      // worker.
      if (version->status() != ServiceWorkerVersion::NEW)
        return false;
      break;
    case RESOURCE_TYPE_SCRIPT:
      // TODO(nhiroki): In the current implementation, importScripts() can be
      // called in any ServiceWorkerVersion::Status except for REDUNDANT, but
      // the spec defines importScripts() works only on the initial script
      // evaluation and the install event. Update this check once
      // importScripts() is fixed (https://crbug.com/719052).
      if (version->status() == ServiceWorkerVersion::REDUNDANT) {
        // This could happen if browser-side has set the status to redundant but
        // the worker has not yet stopped. The worker is already doomed so just
        // reject the request. Handle it specially here because otherwise it'd
        // be unclear whether "REDUNDANT" should count as installed or not
        // installed when making decisions about how to handle the request and
        // logging UMA.
        return false;
      }
      break;
    default:
      // TODO(nhiroki): Record bad message, we shouldn't come here for other
      // request types.
      NOTREACHED();
      return false;
  }

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
