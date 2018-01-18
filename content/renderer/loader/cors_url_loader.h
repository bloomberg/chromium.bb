// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_CORS_URL_LOADER_H_
#define CONTENT_RENDERER_LOADER_CORS_URL_LOADER_H_

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/interfaces/fetch_api.mojom.h"
#include "services/network/public/interfaces/url_loader_factory.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// Wrapper class that adds cross-origin resource sharing capabilities
// (https://fetch.spec.whatwg.org/#http-cors-protocol), delegating requests as
// well as potential preflight requests to the supplied
// |network_loader_factory|. It is owned by the CORSURLLoaderFactory that
// created it.
class CONTENT_EXPORT CORSURLLoader : public network::mojom::URLLoader,
                                     public network::mojom::URLLoaderClient {
 public:
  // Assumes network_loader_factory outlives this loader.
  CORSURLLoader(
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& resource_request,
      network::mojom::URLLoaderClientPtr client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      network::mojom::URLLoaderFactory* network_loader_factory);

  ~CORSURLLoader() override;

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
      const base::Optional<net::SSLInfo>& ssl_info,
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

 private:
  // Called when there is a connection error on the upstream pipe used for the
  // actual request.
  void OnUpstreamConnectionError();

  // Handles OnComplete() callback.
  void HandleComplete(const network::URLLoaderCompletionStatus& status);

  // This raw URLLoaderFactory pointer is shared with the CORSURLLoaderFactory
  // that created and owns this object.
  network::mojom::URLLoaderFactory* network_loader_factory_;

  // For the actual request.
  network::mojom::URLLoaderPtr network_loader_;
  mojo::Binding<network::mojom::URLLoaderClient> network_client_binding_;

  // To be a URLLoader for the client.
  network::mojom::URLLoaderClientPtr forwarding_client_;

  // Request initiator's origin.
  url::Origin security_origin_;

  // The last response URL, that is usually the requested URL, but can be
  // different if redirects happen.
  GURL last_response_url_;

  // A flag to indicate that the instance is waiting for that forwarding_client_
  // calls FollowRedirect.
  bool is_waiting_follow_redirect_call_ = false;

  // Corresponds to the Fetch spec, https://fetch.spec.whatwg.org/.
  network::mojom::FetchRequestMode fetch_request_mode_;
  network::mojom::FetchCredentialsMode fetch_credentials_mode_;
  bool fetch_cors_flag_;

  DISALLOW_COPY_AND_ASSIGN(CORSURLLoader);
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_CORS_URL_LOADER_H_
