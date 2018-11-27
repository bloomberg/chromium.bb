// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_WORKER_WORKER_SCRIPT_FETCH_INITIATOR_H_
#define CONTENT_BROWSER_SHARED_WORKER_WORKER_SCRIPT_FETCH_INITIATOR_H_

#include <memory>
#include <set>
#include <utility>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/common/service_worker/service_worker_provider.mojom.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/url_loader_throttle.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/shared_worker/worker_main_script_load_params.mojom.h"

namespace network {
class SharedURLLoaderFactory;
class SharedURLLoaderFactoryInfo;
}  // namespace network

namespace content {

class AppCacheNavigationHandleCore;
class BrowserContext;
class ServiceWorkerContextWrapper;
class StoragePartitionImpl;
class URLLoaderFactoryBundleInfo;
class URLLoaderFactoryGetter;
struct SubresourceLoaderParams;

// PlzWorker:
// WorkerScriptFetchInitiator is the entry point of browser-side script fetch
// for WorkerScriptFetcher.
class WorkerScriptFetchInitiator {
 public:
  using CompletionCallback = base::OnceCallback<void(
      mojom::ServiceWorkerProviderInfoForSharedWorkerPtr,
      network::mojom::URLLoaderFactoryAssociatedPtrInfo,
      std::unique_ptr<URLLoaderFactoryBundleInfo>,
      blink::mojom::WorkerMainScriptLoadParamsPtr,
      base::Optional<SubresourceLoaderParams>,
      bool)>;

  // Creates a worker script fetcher and starts it. Must be called on the UI
  // thread. |callback| will be called with the result on the UI thread.
  static void Start(
      int process_id,
      const GURL& script_url,
      const url::Origin& request_initiator,
      ResourceType resource_type,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      AppCacheNavigationHandleCore* appcache_handle_core,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      StoragePartitionImpl* storage_partition,
      CompletionCallback callback);

 private:
  static std::unique_ptr<URLLoaderFactoryBundleInfo> CreateFactoryBundle(
      int process_id,
      StoragePartitionImpl* storage_partition,
      bool file_support);
  static void AddAdditionalRequestHeaders(
      network::ResourceRequest* resource_request,
      BrowserContext* browser_context);
  static void CreateScriptLoaderOnIO(
      int process_id,
      std::unique_ptr<network::ResourceRequest> resource_request,
      scoped_refptr<URLLoaderFactoryGetter> loader_factory_getter,
      std::unique_ptr<URLLoaderFactoryBundleInfo>
          factory_bundle_for_browser_info,
      std::unique_ptr<URLLoaderFactoryBundleInfo> subresource_loader_factories,
      scoped_refptr<ServiceWorkerContextWrapper> context,
      AppCacheNavigationHandleCore* appcache_handle_core,
      std::unique_ptr<network::SharedURLLoaderFactoryInfo>
          blob_url_loader_factory_info,
      CompletionCallback callback);
  static void DidCreateScriptLoaderOnIO(
      CompletionCallback callback,
      mojom::ServiceWorkerProviderInfoForSharedWorkerPtr
          service_worker_provider_info,
      network::mojom::URLLoaderFactoryAssociatedPtrInfo
          main_script_loader_factory,
      std::unique_ptr<URLLoaderFactoryBundleInfo> subresource_loader_factories,
      blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
      base::Optional<SubresourceLoaderParams> subresource_loader_params,
      bool success);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_WORKER_WORKER_SCRIPT_FETCH_INITIATOR_H_
