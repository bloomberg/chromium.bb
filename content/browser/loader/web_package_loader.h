// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_WEB_PACKAGE_LOADER_H_
#define CONTENT_BROWSER_LOADER_WEB_PACKAGE_LOADER_H_

#include "base/optional.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/ssl/ssl_info.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/net_adapters.h"
#include "services/network/public/interfaces/url_loader.mojom.h"

namespace content {

class SignedExchangeHandler;
class SourceStreamToDataPipe;

// WebPackageLoader handles an origin-signed HTTP exchange response. It is
// created when a WebPackageRequestHandler recieves an origin-signed HTTP
// exchange response, and is owned by the handler until the StartLoaderCallback
// of WebPackageRequestHandler::StartResponse is called. After that, it is
// owned by the URLLoader mojo endpoint.
class WebPackageLoader final : public network::mojom::URLLoaderClient,
                               public network::mojom::URLLoader {
 public:
  WebPackageLoader(const network::ResourceResponseHead& original_response,
                   network::mojom::URLLoaderClientPtr forwarding_client,
                   network::mojom::URLLoaderClientEndpointsPtr endpoints);
  ~WebPackageLoader() override;

  // network::mojom::URLLoaderClient implementation
  // Only OnStartLoadingResponseBody() and OnComplete() are called.
  void OnReceiveResponse(
      const network::ResourceResponseHead& response_head,
      const base::Optional<net::SSLInfo>& ssl_info,
      network::mojom::DownloadedTempFilePtr downloaded_file) override;
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      const network::ResourceResponseHead& response_head) override;
  void OnDataDownloaded(int64_t data_len, int64_t encoded_data_len) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override;
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  // network::mojom::URLLoader implementation
  void FollowRedirect() override;
  void ProceedWithResponse() override;
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  void ConnectToClient(network::mojom::URLLoaderClientPtr client);

 private:
  class ResponseTimingInfo;

  // Called from |signed_exchange_handler_| when it finds an origin-signed HTTP
  // exchange.
  void OnHTTPExchangeFound(
      const GURL& request_url,
      const std::string& request_method,
      const network::ResourceResponseHead& resource_response,
      base::Optional<net::SSLInfo> ssl_info);

  void FinishReadingBody(int result);

  // This timing info is used to create a dummy redirect response.
  std::unique_ptr<const ResponseTimingInfo> original_response_timing_info_;

  // This client is alive until OnHTTPExchangeFound() is called.
  network::mojom::URLLoaderClientPtr forwarding_client_;

  // This |url_loader_| is the pointer of the network URL loader.
  network::mojom::URLLoaderPtr url_loader_;
  // This binding connects |this| with the network URL loader.
  mojo::Binding<network::mojom::URLLoaderClient> url_loader_client_binding_;

  // This is pending until connected by ConnectToClient().
  network::mojom::URLLoaderClientPtr client_;

  // This URLLoaderClientRequest is used by ConnectToClient() to connect
  // |client_|.
  network::mojom::URLLoaderClientRequest pending_client_request_;

  std::unique_ptr<SignedExchangeHandler> signed_exchange_handler_;
  std::unique_ptr<SourceStreamToDataPipe> body_data_pipe_adapter_;

  // Kept around until ProceedWithResponse is called.
  mojo::ScopedDataPipeConsumerHandle pending_body_consumer_;

  base::WeakPtrFactory<WebPackageLoader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebPackageLoader);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_WEB_PACKAGE_LOADER_H_
