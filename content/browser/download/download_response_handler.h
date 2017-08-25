// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_RESPONSE_HANDLER_
#define CONTENT_BROWSER_DOWNLOAD_RESPONSE_HANDLER_

#include "content/browser/download/download_create_info.h"
#include "content/public/common/url_loader.mojom.h"

namespace content {

class DownloadUrlParameters;

// This class is responsible for handling the server response for a download.
// It passes the DataPipeConsumerHandle and completion status to the download
// sink. The class is common to both navigation triggered downloads and
// context menu downloads
class DownloadResponseHandler : public mojom::URLLoaderClient {
 public:
  class Delegate {
   public:
    virtual void OnResponseStarted(
        std::unique_ptr<DownloadCreateInfo> download_create_info,
        mojo::ScopedDataPipeConsumerHandle body) = 0;
  };

  DownloadResponseHandler(DownloadUrlParameters* params,
                          Delegate* delegate,
                          bool is_parallel_request);
  ~DownloadResponseHandler() override;

  // mojom::URLLoaderClient
  void OnReceiveResponse(const content::ResourceResponseHead& head,
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
  Delegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(DownloadResponseHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_RESPONSE_HANDLER
