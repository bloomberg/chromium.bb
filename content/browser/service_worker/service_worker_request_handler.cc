// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_request_handler.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_navigation_handle_core.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/resource_context.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/url_constants.h"
#include "ipc/ipc_message.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace content {

namespace {

bool SchemeMaySupportRedirectingToHTTPS(const GURL& url) {
#if defined(OS_CHROMEOS)
  return url.SchemeIs(kExternalFileScheme);
#else   // OS_CHROMEOS
  return false;
#endif  // OS_CHROMEOS
}

}  // namespace

// static
std::unique_ptr<NavigationLoaderInterceptor>
ServiceWorkerRequestHandler::InitializeForNavigation(
    const GURL& url,
    ResourceContext* resource_context,
    ServiceWorkerNavigationHandleCore* navigation_handle_core,
    storage::BlobStorageContext* blob_storage_context,
    bool skip_service_worker,
    ResourceType resource_type,
    blink::mojom::RequestContextType request_context_type,
    network::mojom::RequestContextFrameType frame_type,
    bool is_parent_frame_secure,
    scoped_refptr<network::ResourceRequestBody> body,
    base::RepeatingCallback<WebContents*()> web_contents_getter,
    base::WeakPtr<ServiceWorkerProviderHost>* out_provider_host) {
  DCHECK(navigation_handle_core);

  // Create the handler even for insecure HTTP since it's used in the
  // case of redirect to HTTPS.
  if (!url.SchemeIsHTTPOrHTTPS() && !OriginCanAccessServiceWorkers(url) &&
      !SchemeMaySupportRedirectingToHTTPS(url)) {
    return nullptr;
  }

  if (!navigation_handle_core->context_wrapper())
    return nullptr;
  ServiceWorkerContextCore* context =
      navigation_handle_core->context_wrapper()->context();
  if (!context)
    return nullptr;

  auto provider_info = blink::mojom::ServiceWorkerProviderInfoForWindow::New();
  // Initialize the SWProviderHost.
  *out_provider_host = ServiceWorkerProviderHost::PreCreateNavigationHost(
      context->AsWeakPtr(), is_parent_frame_secure,
      std::move(web_contents_getter), &provider_info);

  std::unique_ptr<ServiceWorkerRequestHandler> handler(
      (*out_provider_host)
          ->CreateRequestHandler(
              network::mojom::FetchRequestMode::kNavigate,
              network::mojom::FetchCredentialsMode::kInclude,
              network::mojom::FetchRedirectMode::kManual,
              std::string() /* integrity */, false /* keepalive */,
              resource_type, request_context_type, frame_type,
              blob_storage_context->AsWeakPtr(), body, skip_service_worker));

  navigation_handle_core->OnCreatedProviderHost(*out_provider_host,
                                                std::move(provider_info));

  return base::WrapUnique<NavigationLoaderInterceptor>(handler.release());
}

// static
std::unique_ptr<NavigationLoaderInterceptor>
ServiceWorkerRequestHandler::InitializeForWorker(
    const network::ResourceRequest& resource_request,
    base::WeakPtr<ServiceWorkerProviderHost> host) {
  DCHECK(resource_request.resource_type == RESOURCE_TYPE_WORKER ||
         resource_request.resource_type == RESOURCE_TYPE_SHARED_WORKER)
      << resource_request.resource_type;

  // Create the handler even for insecure HTTP since it's used in the
  // case of redirect to HTTPS.
  if (!resource_request.url.SchemeIsHTTPOrHTTPS() &&
      !OriginCanAccessServiceWorkers(resource_request.url)) {
    return nullptr;
  }

  std::unique_ptr<ServiceWorkerRequestHandler> handler(
      host->CreateRequestHandler(
          resource_request.fetch_request_mode,
          resource_request.fetch_credentials_mode,
          resource_request.fetch_redirect_mode,
          resource_request.fetch_integrity, resource_request.keepalive,
          static_cast<ResourceType>(resource_request.resource_type),
          resource_request.resource_type == RESOURCE_TYPE_WORKER
              ? blink::mojom::RequestContextType::WORKER
              : blink::mojom::RequestContextType::SHARED_WORKER,
          resource_request.fetch_frame_type,
          nullptr /* blob_storage_context: unused in S13n */,
          resource_request.request_body, resource_request.skip_service_worker));

  return base::WrapUnique<NavigationLoaderInterceptor>(handler.release());
}

void ServiceWorkerRequestHandler::MaybeCreateLoader(
    const network::ResourceRequest& tentative_request,
    ResourceContext* resource_context,
    LoaderCallback callback,
    FallbackCallback fallback_callback) {
  NOTREACHED();
  std::move(callback).Run({});
}

ServiceWorkerRequestHandler::~ServiceWorkerRequestHandler() {
}

ServiceWorkerRequestHandler::ServiceWorkerRequestHandler(
    base::WeakPtr<ServiceWorkerContextCore> context,
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
    ResourceType resource_type)
    : context_(context),
      provider_host_(provider_host),
      blob_storage_context_(blob_storage_context),
      resource_type_(resource_type) {}

}  // namespace content
