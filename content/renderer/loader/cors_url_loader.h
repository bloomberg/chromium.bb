// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_CORS_URL_LOADER_H_
#define CONTENT_RENDERER_LOADER_CORS_URL_LOADER_H_

#include "content/public/common/url_loader_factory.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace content {

// Wrapper class that adds cross-origin resource sharing capabilities
// (https://www.w3.org/TR/cors/), delegating requests as well as potential
// preflight requests to the supplied |network_loader_factory|. It is owned by
// the CORSURLLoaderFactory that created it.
class CONTENT_EXPORT CORSURLLoader : public mojom::URLLoader,
                                     public mojom::URLLoaderClient {
 public:
  // Assumes network_loader_factory outlives this loader.
  CORSURLLoader(
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const ResourceRequest& resource_request,
      mojom::URLLoaderClientPtr client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojom::URLLoaderFactory* network_loader_factory);

  ~CORSURLLoader() override;

  // mojom::URLLoader overrides:
  void FollowRedirect() override;
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // mojom::URLLoaderClient overrides:
  void OnReceiveResponse(const ResourceResponseHead& head,
                         const base::Optional<net::SSLInfo>& ssl_info,
                         mojom::DownloadedTempFilePtr downloaded_file) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const ResourceResponseHead& head) override;
  void OnDataDownloaded(int64_t data_length, int64_t encoded_length) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        base::OnceCallback<void()> callback) override;
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(
      const ResourceRequestCompletionStatus& completion_status) override;

  // Called when there is a connection error on the upstream pipe used for the
  // actual request.
  void OnUpstreamConnectionError();

 private:
  // This raw URLLoaderFactory pointer is shared with the CORSURLLoaderFactory
  // that created and owns this object.
  mojom::URLLoaderFactory* network_loader_factory_;

  // For the actual request.
  mojom::URLLoaderPtr network_loader_;
  mojo::Binding<mojom::URLLoaderClient> network_client_binding_;

  // To be a URLLoader for the client.
  mojom::URLLoaderClientPtr forwarding_client_;

  DISALLOW_COPY_AND_ASSIGN(CORSURLLoader);
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_CORS_URL_LOADER_H_
