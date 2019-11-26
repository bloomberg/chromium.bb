// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_PREFETCH_URL_LOADER_H_
#define CONTENT_BROWSER_LOADER_PREFETCH_URL_LOADER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace storage {
class BlobStorageContext;
}  // namespace storage

namespace network {
class SharedURLLoaderFactory;
}

namespace blink {
class URLLoaderThrottle;
}

namespace content {

class BrowserContext;
class PrefetchedSignedExchangeCacheAdapter;
class SignedExchangePrefetchHandler;
class SignedExchangePrefetchMetricRecorder;

// PrefetchURLLoader which basically just keeps draining the data.
class CONTENT_EXPORT PrefetchURLLoader : public network::mojom::URLLoader,
                                         public network::mojom::URLLoaderClient,
                                         public mojo::DataPipeDrainer::Client {
 public:
  using URLLoaderThrottlesGetter = base::RepeatingCallback<
      std::vector<std::unique_ptr<blink::URLLoaderThrottle>>()>;
  using RecursivePrefetchTokenGenerator =
      base::OnceCallback<base::UnguessableToken(
          const network::ResourceRequest&)>;

  // |url_loader_throttles_getter| may be used when a prefetch handler needs to
  // additionally create a request (e.g. for fetching certificate if the
  // prefetch was for a signed exchange).
  PrefetchURLLoader(
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      int frame_tree_node_id,
      const network::ResourceRequest& resource_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory,
      URLLoaderThrottlesGetter url_loader_throttles_getter,
      BrowserContext* browser_context,
      scoped_refptr<SignedExchangePrefetchMetricRecorder>
          signed_exchange_prefetch_metric_recorder,
      scoped_refptr<PrefetchedSignedExchangeCache>
          prefetched_signed_exchange_cache,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
      const std::string& accept_langs,
      RecursivePrefetchTokenGenerator recursive_prefetch_token_generator);
  ~PrefetchURLLoader() override;

  // Sends an empty response's body to |forwarding_client_|. If failed to create
  // a new data pipe, sends ERR_INSUFFICIENT_RESOURCES and closes the
  // connection, and returns false. Otherwise returns true.
  bool SendEmptyBody();

  void SendOnComplete(
      const network::URLLoaderCompletionStatus& completion_status);

 private:
  // This enum is used to record a histogram and should not be renumbered.
  enum class PrefetchRedirect {
    kPrefetchMade = 0,
    kPrefetchRedirected = 1,
    kPrefetchRedirectedSXGHandler = 2,
    kMaxValue = kPrefetchRedirectedSXGHandler
  };

  void RecordPrefetchRedirectHistogram(PrefetchRedirect event);

  // network::mojom::URLLoader overrides:
  void FollowRedirect(const std::vector<std::string>& removed_headers,
                      const net::HttpRequestHeaders& modified_headers,
                      const base::Optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // network::mojom::URLLoaderClient overrides:
  void OnReceiveResponse(network::mojom::URLResponseHeadPtr head) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        base::OnceCallback<void()> callback) override;
  void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  // mojo::DataPipeDrainer::Client overrides:
  // This just does nothing but keep reading.
  void OnDataAvailable(const void* data, size_t num_bytes) override {}
  void OnDataComplete() override {}

  void OnNetworkConnectionError();

  const int frame_tree_node_id_;

  // Set in the constructor and updated when redirected.
  network::ResourceRequest resource_request_;

  scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory_;

  // For the actual request.
  mojo::Remote<network::mojom::URLLoader> loader_;
  mojo::Receiver<network::mojom::URLLoaderClient> client_receiver_{this};

  // To be a URLLoader for the client.
  mojo::Remote<network::mojom::URLLoaderClient> forwarding_client_;

  URLLoaderThrottlesGetter url_loader_throttles_getter_;

  std::unique_ptr<mojo::DataPipeDrainer> pipe_drainer_;

  std::unique_ptr<SignedExchangePrefetchHandler>
      signed_exchange_prefetch_handler_;

  scoped_refptr<SignedExchangePrefetchMetricRecorder>
      signed_exchange_prefetch_metric_recorder_;

  // Used when SignedExchangeSubresourcePrefetch is enabled to store the
  // prefetched signed exchanges to a PrefetchedSignedExchangeCache.
  std::unique_ptr<PrefetchedSignedExchangeCacheAdapter>
      prefetched_signed_exchange_cache_adapter_;

  const std::string accept_langs_;

  RecursivePrefetchTokenGenerator recursive_prefetch_token_generator_;

  // TODO(kinuko): This value can become stale if the preference is updated.
  // Make this listen to the changes if it becomes a real concern.
  bool is_signed_exchange_handling_enabled_ = false;

  DISALLOW_COPY_AND_ASSIGN(PrefetchURLLoader);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_PREFETCH_URL_LOADER_H_
