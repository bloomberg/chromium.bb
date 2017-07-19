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
#include "content/public/common/resource_request_body.h"
#include "content/public/common/service_worker_modes.h"
#include "ipc/ipc_message.h"
#include "net/base/url_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_interceptor.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace content {

namespace {

int kUserDataKey;  // Key value is not important.

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
      return NULL;
    return handler->MaybeCreateJob(
        request, network_delegate, resource_context_);
  }

 private:
  ResourceContext* resource_context_;
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRequestInterceptor);
};

}  // namespace

// PlzNavigate
void ServiceWorkerRequestHandler::InitializeForNavigation(
    net::URLRequest* request,
    ServiceWorkerNavigationHandleCore* navigation_handle_core,
    storage::BlobStorageContext* blob_storage_context,
    bool skip_service_worker,
    ResourceType resource_type,
    RequestContextType request_context_type,
    RequestContextFrameType frame_type,
    bool is_parent_frame_secure,
    scoped_refptr<ResourceRequestBody> body,
    const base::Callback<WebContents*(void)>& web_contents_getter) {
  CHECK(IsBrowserSideNavigationEnabled());

  // Only create a handler when there is a ServiceWorkerNavigationHandlerCore
  // to take ownership of a pre-created SeviceWorkerProviderHost.
  if (!navigation_handle_core)
    return;

  // Create the handler even for insecure HTTP since it's used in the
  // case of redirect to HTTPS.
  if (!request->url().SchemeIsHTTPOrHTTPS() &&
      !OriginCanAccessServiceWorkers(request->url())) {
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
          FETCH_REQUEST_MODE_NAVIGATE, FETCH_CREDENTIALS_MODE_INCLUDE,
          FetchRedirectMode::MANUAL_MODE, std::string() /* integrity */,
          resource_type, request_context_type, frame_type,
          blob_storage_context->AsWeakPtr(), body, skip_service_worker));
  if (handler)
    request->SetUserData(&kUserDataKey, std::move(handler));

  // Transfer ownership to the ServiceWorkerNavigationHandleCore.
  // In the case of a successful navigation, the SWProviderHost will be
  // transferred to its "final" destination in the OnProviderCreated handler. If
  // the navigation fails, it will be destroyed along with the
  // ServiceWorkerNavigationHandleCore.
  navigation_handle_core->DidPreCreateProviderHost(std::move(provider_host));
}

// PlzNavigate and --enable-network-service.
// static
std::unique_ptr<URLLoaderRequestHandler>
ServiceWorkerRequestHandler::InitializeForNavigationNetworkService(
    const ResourceRequest& resource_request,
    ResourceContext* resource_context,
    ServiceWorkerNavigationHandleCore* navigation_handle_core,
    storage::BlobStorageContext* blob_storage_context,
    bool skip_service_worker,
    ResourceType resource_type,
    RequestContextType request_context_type,
    RequestContextFrameType frame_type,
    bool is_parent_frame_secure,
    scoped_refptr<ResourceRequestBody> body,
    const base::Callback<WebContents*(void)>& web_contents_getter) {
  DCHECK(IsBrowserSideNavigationEnabled() &&
         base::FeatureList::IsEnabled(features::kNetworkService));
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
          FETCH_REQUEST_MODE_NAVIGATE, FETCH_CREDENTIALS_MODE_INCLUDE,
          FetchRedirectMode::MANUAL_MODE, std::string() /* integrity */,
          resource_type, request_context_type, frame_type,
          blob_storage_context->AsWeakPtr(), body, skip_service_worker));

  // Transfer ownership to the ServiceWorkerNavigationHandleCore.
  // In the case of a successful navigation, the SWProviderHost will be
  // transferred to its "final" destination in the OnProviderCreated handler. If
  // the navigation fails, it will be destroyed along with the
  // ServiceWorkerNavigationHandleCore.
  navigation_handle_core->DidPreCreateProviderHost(std::move(provider_host));

  return base::WrapUnique<URLLoaderRequestHandler>(handler.release());
}

void ServiceWorkerRequestHandler::InitializeHandler(
    net::URLRequest* request,
    ServiceWorkerContextWrapper* context_wrapper,
    storage::BlobStorageContext* blob_storage_context,
    int process_id,
    int provider_id,
    bool skip_service_worker,
    FetchRequestMode request_mode,
    FetchCredentialsMode credentials_mode,
    FetchRedirectMode redirect_mode,
    const std::string& integrity,
    ResourceType resource_type,
    RequestContextType request_context_type,
    RequestContextFrameType frame_type,
    scoped_refptr<ResourceRequestBody> body) {
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
          request_mode, credentials_mode, redirect_mode, integrity,
          resource_type, request_context_type, frame_type,
          blob_storage_context->AsWeakPtr(), body, skip_service_worker));
  if (handler)
    request->SetUserData(&kUserDataKey, std::move(handler));
}

ServiceWorkerRequestHandler* ServiceWorkerRequestHandler::GetHandler(
    const net::URLRequest* request) {
  return static_cast<ServiceWorkerRequestHandler*>(
      request->GetUserData(&kUserDataKey));
}

std::unique_ptr<net::URLRequestInterceptor>
ServiceWorkerRequestHandler::CreateInterceptor(
    ResourceContext* resource_context) {
  return std::unique_ptr<net::URLRequestInterceptor>(
      new ServiceWorkerRequestInterceptor(resource_context));
}

bool ServiceWorkerRequestHandler::IsControlledByServiceWorker(
    const net::URLRequest* request) {
  ServiceWorkerRequestHandler* handler = GetHandler(request);
  if (!handler || !handler->provider_host_)
    return false;
  return handler->provider_host_->associated_registration() ||
         handler->provider_host_->running_hosted_version();
}

ServiceWorkerProviderHost* ServiceWorkerRequestHandler::GetProviderHost(
    const net::URLRequest* request) {
  ServiceWorkerRequestHandler* handler = GetHandler(request);
  return handler ? handler->provider_host_.get() : nullptr;
}

void ServiceWorkerRequestHandler::MaybeCreateLoader(
    const ResourceRequest& request,
    ResourceContext* resource_context,
    LoaderCallback callback) {
  NOTREACHED();
  std::move(callback).Run(StartLoaderCallback());
}

void ServiceWorkerRequestHandler::PrepareForCrossSiteTransfer(
    int old_process_id) {
  CHECK(!IsBrowserSideNavigationEnabled());
  if (!provider_host_ || !context_)
    return;
  old_process_id_ = old_process_id;
  old_provider_id_ = provider_host_->provider_id();
  host_for_cross_site_transfer_ = context_->TransferProviderHostOut(
      old_process_id, provider_host_->provider_id());
  DCHECK_EQ(provider_host_.get(), host_for_cross_site_transfer_.get());
}

void ServiceWorkerRequestHandler::CompleteCrossSiteTransfer(
    int new_process_id, int new_provider_id) {
  CHECK(!IsBrowserSideNavigationEnabled());
  if (!host_for_cross_site_transfer_.get() || !context_)
    return;
  DCHECK_EQ(provider_host_.get(), host_for_cross_site_transfer_.get());
  context_->TransferProviderHostIn(new_process_id, new_provider_id,
                                   std::move(host_for_cross_site_transfer_));
  DCHECK_EQ(provider_host_->provider_id(), new_provider_id);
}

void ServiceWorkerRequestHandler::MaybeCompleteCrossSiteTransferInOldProcess(
    int old_process_id) {
  CHECK(!IsBrowserSideNavigationEnabled());
  if (!host_for_cross_site_transfer_.get() || !context_ ||
      old_process_id_ != old_process_id) {
    return;
  }
  CompleteCrossSiteTransfer(old_process_id_, old_provider_id_);
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
      resource_type_(resource_type),
      old_process_id_(0),
      old_provider_id_(kInvalidServiceWorkerProviderId) {
}

}  // namespace content
