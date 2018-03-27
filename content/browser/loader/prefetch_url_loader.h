// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_PREFETCH_URL_LOADER_H_
#define CONTENT_BROWSER_LOADER_PREFETCH_URL_LOADER_H_

#include <memory>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "mojo/common/data_pipe_drainer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace net {
class URLRequestContextGetter;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace content {

class ResourceContext;
class URLLoaderThrottle;
class WebPackagePrefetchHandler;

// PrefetchURLLoader which basically just keeps draining the data.
class CONTENT_EXPORT PrefetchURLLoader
    : public network::mojom::URLLoader,
      public network::mojom::URLLoaderClient,
      public mojo::common::DataPipeDrainer::Client {
 public:
  using URLLoaderThrottlesGetter = base::RepeatingCallback<
      std::vector<std::unique_ptr<content::URLLoaderThrottle>>()>;

  // |url_loader_throttles_getter|, |resource_context| and
  // |request_context_getter| may be used when a prefetch handler
  // needs to additionally create a request (e.g. for fetching certificate
  // if the prefetch was for a signed exchange).
  PrefetchURLLoader(
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& resource_request,
      network::mojom::URLLoaderClientPtr client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory,
      URLLoaderThrottlesGetter url_loader_throttles_getter,
      ResourceContext* resource_context,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter);
  ~PrefetchURLLoader() override;

 private:
  // network::mojom::URLLoader overrides:
  void FollowRedirect() override;
  void ProceedWithResponse() override;
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // network::mojom::URLLoaderClient overrides:
  void OnReceiveResponse(
      const network::ResourceResponseHead& head,
      network::mojom::DownloadedTempFilePtr downloaded_file) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const network::ResourceResponseHead& head) override;
  void OnDataDownloaded(int64_t data_length, int64_t encoded_length) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        base::OnceCallback<void()> callback) override;
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  // mojo::common::DataPipeDrainer::Client overrides:
  // This just does nothing but keep reading.
  void OnDataAvailable(const void* data, size_t num_bytes) override {}
  void OnDataComplete() override {}

  void OnNetworkConnectionError();

  scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory_;

  // For the actual request.
  network::mojom::URLLoaderPtr loader_;
  mojo::Binding<network::mojom::URLLoaderClient> client_binding_;

  // To be a URLLoader for the client.
  network::mojom::URLLoaderClientPtr forwarding_client_;

  url::Origin request_initiator_;

  // |url_loader_throttles_getter_| and |resource_context_| should be
  // valid as far as |request_context_getter_| returns non-null value.
  URLLoaderThrottlesGetter url_loader_throttles_getter_;
  ResourceContext* resource_context_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  std::unique_ptr<mojo::common::DataPipeDrainer> pipe_drainer_;

  std::unique_ptr<WebPackagePrefetchHandler> web_package_prefetch_handler_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchURLLoader);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_PREFETCH_URL_LOADER_H_
