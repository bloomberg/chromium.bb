// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_RESPONSE_HANDLER_
#define CONTENT_BROWSER_DOWNLOAD_RESPONSE_HANDLER_

#include <vector>

#include "content/browser/download/download_create_info.h"
#include "content/public/common/download_stream.mojom.h"
#include "content/public/common/referrer.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/url_loader.mojom.h"
#include "net/cert/cert_status_flags.h"

namespace content {

struct DownloadCreateInfo;
struct DownloadSaveInfo;

// This class is responsible for handling the server response for a download.
// It passes the DataPipeConsumerHandle and completion status to the download
// sink. The class is common to both navigation triggered downloads and
// context menu downloads
class DownloadResponseHandler : public mojom::URLLoaderClient {
 public:
  // Class for handling the stream once response starts.
  class Delegate {
   public:
    virtual void OnResponseStarted(
        std::unique_ptr<DownloadCreateInfo> download_create_info,
        mojom::DownloadStreamHandlePtr stream_handle) = 0;
    virtual void OnReceiveRedirect() = 0;
  };

  DownloadResponseHandler(ResourceRequest* resource_request,
                          Delegate* delegate,
                          std::unique_ptr<DownloadSaveInfo> save_info,
                          bool is_parallel_request,
                          bool is_transient);
  ~DownloadResponseHandler() override;

  // mojom::URLLoaderClient
  void OnReceiveResponse(const ResourceResponseHead& head,
                         const base::Optional<net::SSLInfo>& ssl_info,
                         mojom::DownloadedTempFilePtr downloaded_file) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const content::ResourceResponseHead& head) override;
  void OnDataDownloaded(int64_t data_length, int64_t encoded_length) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const content::ResourceRequestCompletionStatus&
                  completion_status) override;

 private:
  std::unique_ptr<DownloadCreateInfo> CreateDownloadCreateInfo(
      const ResourceResponseHead& head);

  // Helper method that is called when response is received.
  void OnResponseStarted(mojom::DownloadStreamHandlePtr stream_handle);

  Delegate* const delegate_;

  std::unique_ptr<DownloadCreateInfo> create_info_;

  bool started_;

  // Information needed to create DownloadCreateInfo when the time comes.
  std::unique_ptr<DownloadSaveInfo> save_info_;
  std::vector<GURL> url_chain_;
  std::string method_;
  GURL referrer_;
  bool is_transient_;
  net::CertStatus cert_status_;
  bool has_strong_validators_;
  GURL origin_;

  // Mojo interface ptr to send the completion status to the download sink.
  mojom::DownloadStreamClientPtr client_ptr_;

  DISALLOW_COPY_AND_ASSIGN(DownloadResponseHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_RESPONSE_HANDLER
