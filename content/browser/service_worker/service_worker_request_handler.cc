// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_request_handler.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "content/browser/loader/url_loader_request_handler.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_navigation_handle_core.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_url_request_job.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/resource_context.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/service_worker_modes.h"
#include "content/public/common/url_constants.h"
#include "ipc/ipc_message.h"
#include "net/base/url_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_interceptor.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace content {

namespace {

class ServiceWorkerRequestInterceptor
    : public net::URLRequestInterceptor {
 public:
  explicit ServiceWorkerRequestInterceptor(ResourceContext* resource_context)
      : resource_context_(resource_context) {}
  ~ServiceWorkerRequestInterceptor() override {}
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    ServiceWorkerRequestHandler* handler =
        ServiceWorkerRequestHandler::GetHandler(request);
    if (!handler)
      return nullptr;
    return handler->MaybeCreateJob(
        request, network_delegate, resource_context_);
  }

 private:
  ResourceContext* resource_context_;
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRequestInterceptor);
};

bool SchemeMaySupportRedirectingToHTTPS(const GURL& url) {
#if defined(OS_CHROMEOS)
  return url.SchemeIs(kExternalFileScheme);
#else   // OS_CHROMEOS
  return false;
#endif  // OS_CHROMEOS
}

}  // namespace

// static
int ServiceWorkerRequestHandler::user_data_key_;

// PlzNavigate:
// static
void ServiceWorkerRequestHandler::InitializeForNavigation(
    net::URLRequest* request,
    ServiceWorkerNavigationHandleCore* navigation_handle_core,
    storage::BlobStorageContext* blob_storage_context,
    bool skip_service_worker,
    ResourceType resource_type,
    RequestContextType request_context_type,
    network::mojom::RequestContextFrameType frame_type,
    bool is_parent_frame_secure,
    scoped_refptr<network::ResourceRequestBody> body,
    const base::Callback<WebContents*(void)>& web_contents_getter) {
  CHECK(IsBrowserSideNavigationEnabled());

  // Only create a handler when there is a ServiceWorkerNavigationHandlerCore
  // to take ownership of a pre-created SeviceWorkerProviderHost.
  if (!navigation_handle_core)
    return;

  // Create the handler even for insecure HTTP since it's used in the
  // case of redirect to HTTPS.
  if (!request->url().SchemeIsHTTPOrHTTPS() &&
      !OriginCanAccessServiceWorkers(request->url()) &&
      !SchemeMaySupportRedirectingToHTTPS(request->url())) {
    return;
  }

  if (!navigation_handle_core->context_wrapper() ||
      !navigation_handle_core->context_wrapper()->context()) {
    return;
  }

  // Initialize the SWProviderHost.
  std::unique_ptr<ServiceWorkerProviderHost> provider_host =
      ServiceWorkerProviderHost::PreCreateNavigationHost(
          navigation_handle_core->context_wrapper()->context()->AsWeakPtr(),
          is_parent_frame_secure, web_contents_getter);

  std::unique_ptr<ServiceWorkerRequestHandler> handler(
      provider_host->CreateRequestHandler(
          network::mojom::FetchRequestMode::kNavigate,
          network::mojom::FetchCredentialsMode::kInclude,
          network::mojom::FetchRedirectMode::kManual,
          std::string() /* integrity */, false /* keepalive */, resource_type,
          request_context_type, frame_type, blob_storage_context->AsWeakPtr(),
          body, skip_service_worker));
  if (handler)
    request->SetUserData(&user_data_key_, std::move(handler));

  // Transfer ownership to the ServiceWorkerNavigationHandleCore.
  // In the case of a successful navigation, the SWProviderHost will be
  // transferred to its "final" destination in the OnProviderCreated handler. If
  // the navigation fails, it will be destroyed along with the
  // ServiceWorkerNavigationHandleCore.
  navigation_handle_core->DidPreCreateProviderHost(std::move(provider_host));
}

// S13nServiceWorker:
// static
std::unique_ptr<URLLoaderRequestHandler>
ServiceWorkerRequestHandler::InitializeForNavigationNetworkService(
    const network::ResourceRequest& resource_request,
    ResourceContext* resource_context,
    ServiceWorkerNavigationHandleCore* navigation_handle_core,
    storage::BlobStorageContext* blob_storage_context,
    bool skip_service_worker,
    ResourceType resource_type,
    RequestContextType request_context_type,
    network::mojom::RequestContextFrameType frame_type,
    bool is_parent_frame_secure,
    scoped_refptr<network::ResourceRequestBody> body,
    const base::Callback<WebContents*(void)>& web_contents_getter) {
  DCHECK(ServiceWorkerUtils::IsServicificationEnabled());
  DCHECK(navigation_handle_core);

  // Create the handler even for insecure HTTP since it's used in the
  // case of redirect to HTTPS.
  if (!resource_request.url.SchemeIsHTTPOrHTTPS() &&
      !OriginCanAccessServiceWorkers(resource_request.url)) {
    return nullptr;
  }

  if (!navigation_handle_core->context_wrapper() ||
      !navigation_handle_core->context_wrapper()->context()) {
    return nullptr;
  }

  // Initialize the SWProviderHost.
  std::unique_ptr<ServiceWorkerProviderHost> provider_host =
      ServiceWorkerProviderHost::PreCreateNavigationHost(
          navigation_handle_core->context_wrapper()->context()->AsWeakPtr(),
          is_parent_frame_secure, web_contents_getter);

  std::unique_ptr<ServiceWorkerRequestHandler> handler(
      provider_host->CreateRequestHandler(
          network::mojom::FetchRequestMode::kNavigate,
          network::mojom::FetchCredentialsMode::kInclude,
          network::mojom::FetchRedirectMode::kManual,
          std::string() /* integrity */, false /* keepalive */, resource_type,
          request_context_type, frame_type, blob_storage_context->AsWeakPtr(),
          body, skip_service_worker));

  // Transfer ownership to the ServiceWorkerNavigationHandleCore.
  // In the case of a successful navigation, the SWProviderHost will be
  // transferred to its "final" destination in the OnProviderCreated handler. If
  // the navigation fails, it will be destroyed along with the
  // ServiceWorkerNavigationHandleCore.
  navigation_handle_core->DidPreCreateProviderHost(std::move(provider_host));

  return base::WrapUnique<URLLoaderRequestHandler>(handler.release());
}

// static
void ServiceWorkerRequestHandler::InitializeHandler(
    net::URLRequest* request,
    ServiceWorkerContextWrapper* context_wrapper,
    storage::BlobStorageContext* blob_storage_context,
    int process_id,
    int provider_id,
    bool skip_service_worker,
    network::mojom::FetchRequestMode request_mode,
    network::mojom::FetchCredentialsMode credentials_mode,
    network::mojom::FetchRedirectMode redirect_mode,
    const std::string& integrity,
    bool keepalive,
    ResourceType resource_type,
    RequestContextType request_context_type,
    network::mojom::RequestContextFrameType frame_type,
    scoped_refptr<network::ResourceRequestBody> body) {
  // Create the handler even for insecure HTTP since it's used in the
  // case of redirect to HTTPS.
  if (!request->url().SchemeIsHTTPOrHTTPS() &&
      !OriginCanAccessServiceWorkers(request->url())) {
    return;
  }

  if (!context_wrapper || !context_wrapper->context() ||
      provider_id == kInvalidServiceWorkerProviderId) {
    return;
  }

  ServiceWorkerProviderHost* provider_host =
      context_wrapper->context()->GetProviderHost(process_id, provider_id);
  if (!provider_host || !provider_host->IsContextAlive())
    return;

  std::unique_ptr<ServiceWorkerRequestHandler> handler(
      provider_host->CreateRequestHandler(
          request_mode, credentials_mode, redirect_mode, integrity, keepalive,
          resource_type, request_context_type, frame_type,
          blob_storage_context->AsWeakPtr(), body, skip_service_worker));
  if (handler)
    request->SetUserData(&user_data_key_, std::move(handler));
}

// static
ServiceWorkerRequestHandler* ServiceWorkerRequestHandler::GetHandler(
    const net::URLRequest* request) {
  return static_cast<ServiceWorkerRequestHandler*>(
      request->GetUserData(&user_data_key_));
}

// static
std::unique_ptr<net::URLRequestInterceptor>
ServiceWorkerRequestHandler::CreateInterceptor(
    ResourceContext* resource_context) {
  return std::unique_ptr<net::URLRequestInterceptor>(
      new ServiceWorkerRequestInterceptor(resource_context));
}

// static
bool ServiceWorkerRequestHandler::IsControlledByServiceWorker(
    const net::URLRequest* request) {
  ServiceWorkerRequestHandler* handler = GetHandler(request);
  if (!handler || !handler->provider_host_)
    return false;
  return handler->provider_host_->associated_registration() ||
         handler->provider_host_->running_hosted_version();
}

// static
ServiceWorkerProviderHost* ServiceWorkerRequestHandler::GetProviderHost(
    const net::URLRequest* request) {
  ServiceWorkerRequestHandler* handler = GetHandler(request);
  return handler ? handler->provider_host_.get() : nullptr;
}

void ServiceWorkerRequestHandler::MaybeCreateLoader(
    const network::ResourceRequest& request,
    ResourceContext* resource_context,
    LoaderCallback callback) {
  NOTREACHED();
  std::move(callback).Run(StartLoaderCallback());
}

void ServiceWorkerRequestHandler::PrepareForCrossSiteTransfer(
    int old_process_id) {
  CHECK(!IsBrowserSideNavigationEnabled());
}

void ServiceWorkerRequestHandler::CompleteCrossSiteTransfer(
    int new_process_id, int new_provider_id) {
  CHECK(!IsBrowserSideNavigationEnabled());
}

void ServiceWorkerRequestHandler::MaybeCompleteCrossSiteTransferInOldProcess(
    int old_process_id) {
  CHECK(!IsBrowserSideNavigationEnabled());
}

bool ServiceWorkerRequestHandler::SanityCheckIsSameContext(
    ServiceWorkerContextWrapper* wrapper) {
  if (!wrapper)
    return !context_;
  return context_.get() == wrapper->context();
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
