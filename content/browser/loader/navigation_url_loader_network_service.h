// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_NETWORK_SERVICE_H_
#define CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_NETWORK_SERVICE_H_

#include "base/macros.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/common/url_loader.mojom.h"
#include "content/common/url_loader_factory.mojom.h"
#include "content/public/browser/ssl_status.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/connector.h"

namespace net {
struct RedirectInfo;
}

namespace content {

class ResourceContext;
class NavigationPostDataHandler;

// This is an implementation of NavigationURLLoader used when
// --enable-network-service is used.
class NavigationURLLoaderNetworkService : public NavigationURLLoader,
                                          public mojom::URLLoaderClient {
 public:
  // The caller is responsible for ensuring that |delegate| outlives the loader.
  NavigationURLLoaderNetworkService(
      ResourceContext* resource_context,
      StoragePartition* storage_partition,
      std::unique_ptr<NavigationRequestInfo> request_info,
      std::unique_ptr<NavigationUIData> navigation_ui_data,
      ServiceWorkerNavigationHandle* service_worker_handle,
      AppCacheNavigationHandle* appcache_handle,
      NavigationURLLoaderDelegate* delegate);
  ~NavigationURLLoaderNetworkService() override;

  // NavigationURLLoader implementation:
  void FollowRedirect() override;
  void ProceedWithResponse() override;

  // mojom::URLLoaderClient implementation:
  void OnReceiveResponse(const ResourceResponseHead& head,
                         const base::Optional<net::SSLInfo>& ssl_info,
                         mojom::DownloadedTempFilePtr downloaded_file) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const ResourceResponseHead& head) override;
  void OnDataDownloaded(int64_t data_length, int64_t encoded_length) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(
      const ResourceRequestCompletionStatus& completion_status) override;

 private:
  class URLLoaderRequestController;

  NavigationURLLoaderDelegate* delegate_;

  mojo::Binding<mojom::URLLoaderClient> binding_;
  mojom::URLLoaderPtr url_loader_ptr_;
  scoped_refptr<ResourceResponse> response_;
  SSLStatus ssl_status_;

  // Lives on the IO thread.
  std::unique_ptr<URLLoaderRequestController> request_controller_;

  DISALLOW_COPY_AND_ASSIGN(NavigationURLLoaderNetworkService);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_NETWORK_SERVICE_H_
