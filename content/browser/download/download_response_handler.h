// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_RESPONSE_HANDLER_
#define CONTENT_BROWSER_DOWNLOAD_RESPONSE_HANDLER_

#include <vector>

#include "components/download/public/common/download_create_info.h"
#include "components/download/public/common/download_source.h"
#include "components/download/public/common/download_stream.mojom.h"
#include "content/public/common/referrer.h"
#include "net/cert/cert_status_flags.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace download {
struct DownloadCreateInfo;
}  // namespace download

namespace content {

// This class is responsible for handling the server response for a download.
// It passes the DataPipeConsumerHandle and completion status to the download
// sink. The class is common to both navigation triggered downloads and
// context menu downloads
class DownloadResponseHandler : public network::mojom::URLLoaderClient {
 public:
  // Class for handling the stream once response starts.
  class Delegate {
   public:
    virtual void OnResponseStarted(
        std::unique_ptr<download::DownloadCreateInfo> download_create_info,
        download::mojom::DownloadStreamHandlePtr stream_handle) = 0;
    virtual void OnReceiveRedirect() = 0;
  };

  DownloadResponseHandler(network::ResourceRequest* resource_request,
                          Delegate* delegate,
                          std::unique_ptr<download::DownloadSaveInfo> save_info,
                          bool is_parallel_request,
                          bool is_transient,
                          bool fetch_error_body,
                          const std::string& request_origin,
                          download::DownloadSource download_source,
                          std::vector<GURL> url_chain);
  ~DownloadResponseHandler() override;

  // network::mojom::URLLoaderClient
  void OnReceiveResponse(
      const network::ResourceResponseHead& head,
      const base::Optional<net::SSLInfo>& ssl_info,
      network::mojom::DownloadedTempFilePtr downloaded_file) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const network::ResourceResponseHead& head) override;
  void OnDataDownloaded(int64_t data_length, int64_t encoded_length) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

 private:
  std::unique_ptr<download::DownloadCreateInfo> CreateDownloadCreateInfo(
      const network::ResourceResponseHead& head);

  // Helper method that is called when response is received.
  void OnResponseStarted(
      download::mojom::DownloadStreamHandlePtr stream_handle);

  Delegate* const delegate_;

  std::unique_ptr<download::DownloadCreateInfo> create_info_;

  bool started_;

  // Information needed to create DownloadCreateInfo when the time comes.
  std::unique_ptr<download::DownloadSaveInfo> save_info_;
  std::vector<GURL> url_chain_;
  std::string method_;
  GURL referrer_;
  bool is_transient_;
  bool fetch_error_body_;
  std::string request_origin_;
  download::DownloadSource download_source_;
  net::CertStatus cert_status_;
  bool has_strong_validators_;
  GURL origin_;
  bool is_partial_request_;

  // The abort reason if this class decides to block the download.
  download::DownloadInterruptReason abort_reason_;

  // Mojo interface ptr to send the completion status to the download sink.
  download::mojom::DownloadStreamClientPtr client_ptr_;

  DISALLOW_COPY_AND_ASSIGN(DownloadResponseHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_RESPONSE_HANDLER
