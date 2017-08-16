// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_URL_LOADER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_URL_LOADER_H_

#include "base/macros.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/public/common/resource_request.h"
#include "content/public/common/url_loader.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace storage {
class BlobStorageContext;
}  // namespace storage

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerProviderHost;
class URLLoaderFactoryGetter;

// S13nServiceWorker:
// Used by a Service Worker for script loading only during the installation
// time. For now this is just a proxy loader for the network loader.
// Eventually this should replace the existing URLRequestJob-based request
// interception for script loading, namely ServiceWorkerWriteToCacheJob.
// TODO(kinuko): Implement this.
class ServiceWorkerScriptURLLoader : public mojom::URLLoader,
                                     public mojom::URLLoaderClient {
 public:
  ServiceWorkerScriptURLLoader(
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const ResourceRequest& resource_request,
      mojom::URLLoaderClientPtr client,
      base::WeakPtr<ServiceWorkerContextCore> context,
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
      scoped_refptr<URLLoaderFactoryGetter> loader_factory_getter,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation);
  ~ServiceWorkerScriptURLLoader() override;

  // mojom::URLLoader:
  void FollowRedirect() override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;

  // mojom::URLLoaderClient for simply proxying network:
  void OnReceiveResponse(const ResourceResponseHead& response_head,
                         const base::Optional<net::SSLInfo>& ssl_info,
                         mojom::DownloadedTempFilePtr downloaded_file) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const ResourceResponseHead& response_head) override;
  void OnDataDownloaded(int64_t data_len, int64_t encoded_data_len) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override;
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const ResourceRequestCompletionStatus& status) override;

 private:
  mojom::URLLoaderPtr network_loader_;
  mojo::Binding<mojom::URLLoaderClient> network_client_binding_;
  mojom::URLLoaderClientPtr forwarding_client_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerScriptURLLoader);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_URL_LOADER_H_
