// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REQUEST_HANDLER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REQUEST_HANDLER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/resource_type.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/request_context_frame_type.mojom.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"

namespace network {
class ResourceRequestBody;
}

namespace storage {
class BlobStorageContext;
}

namespace content {

class ResourceContext;
class ServiceWorkerContextCore;
class ServiceWorkerNavigationHandleCore;
class ServiceWorkerProviderHost;
class WebContents;

// Abstract base class for routing network requests to ServiceWorkers.
// Created one per URLRequest and attached to each request.
class CONTENT_EXPORT ServiceWorkerRequestHandler
    : public base::SupportsUserData::Data,
      public NavigationLoaderInterceptor {
 public:
  // Returns a loader interceptor for a navigation. May return nullptr
  // if the navigation cannot use service workers.
  // TODO(falken): Rename to InitializeForNavigation since this also is called
  // when NetworkService is disabled.
  static std::unique_ptr<NavigationLoaderInterceptor>
  InitializeForNavigationNetworkService(
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
      base::WeakPtr<ServiceWorkerProviderHost>* out_provider_host);

  static std::unique_ptr<NavigationLoaderInterceptor> InitializeForWorker(
      const network::ResourceRequest& resource_request,
      base::WeakPtr<ServiceWorkerProviderHost> host);

  ~ServiceWorkerRequestHandler() override;

  // NavigationLoaderInterceptor overrides.
  void MaybeCreateLoader(const network::ResourceRequest& tentative_request,
                         ResourceContext* resource_context,
                         LoaderCallback callback,
                         FallbackCallback fallback_callback) override;

 protected:
  ServiceWorkerRequestHandler(
      base::WeakPtr<ServiceWorkerContextCore> context,
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
      ResourceType resource_type);

  base::WeakPtr<ServiceWorkerContextCore> context_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  base::WeakPtr<storage::BlobStorageContext> blob_storage_context_;
  ResourceType resource_type_;

 private:
  static int user_data_key_;  // Only address is used.

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRequestHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REQUEST_HANDLER_H_
