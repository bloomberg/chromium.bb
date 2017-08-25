// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_response_handler.h"

#include "content/browser/download/download_stats.h"
#include "content/browser/download/download_utils.h"
#include "content/public/browser/download_url_parameters.h"

namespace content {

DownloadResponseHandler::DownloadResponseHandler(DownloadUrlParameters* params,
                                                 Delegate* delegate,
                                                 bool is_parallel_request)
    : delegate_(delegate) {
  if (!is_parallel_request)
    RecordDownloadCount(UNTHROTTLED_COUNT);

  // TODO(qinmin): create the DownloadSaveInfo from |params|
}

DownloadResponseHandler::~DownloadResponseHandler() = default;

void DownloadResponseHandler::OnReceiveResponse(
    const content::ResourceResponseHead& head,
    const base::Optional<net::SSLInfo>& ssl_info,
    mojom::DownloadedTempFilePtr downloaded_file) {
  // TODO(qinmin): create the DownloadCreateInfo from |head|.

  // TODO(qinmin): pass DownloadSaveInfo here.
  DownloadInterruptReason result =
      head.headers
          ? HandleSuccessfulServerResponse(*(head.headers.get()), nullptr)
          : DOWNLOAD_INTERRUPT_REASON_NONE;
  if (result != DOWNLOAD_INTERRUPT_REASON_NONE) {
    delegate_->OnResponseStarted(base::MakeUnique<DownloadCreateInfo>(),
                                 mojo::ScopedDataPipeConsumerHandle());
  }
}

void DownloadResponseHandler::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const content::ResourceResponseHead& head) {}

void DownloadResponseHandler::OnDataDownloaded(int64_t data_length,
                                               int64_t encoded_length) {}

void DownloadResponseHandler::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback callback) {}

void DownloadResponseHandler::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {}

void DownloadResponseHandler::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {};

void DownloadResponseHandler::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  delegate_->OnResponseStarted(base::MakeUnique<DownloadCreateInfo>(),
                               std::move(body));
}

void DownloadResponseHandler::OnComplete(
    const content::ResourceRequestCompletionStatus& completion_status) {
  // TODO(qinmin): passing the |completion_status| to DownloadFile.
}

}  // namespace content
