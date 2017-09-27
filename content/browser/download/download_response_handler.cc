// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_response_handler.h"

#include "content/browser/download/download_stats.h"
#include "content/browser/download/download_utils.h"
#include "content/public/browser/download_url_parameters.h"
#include "net/http/http_status_code.h"
#include "net/log/net_log_with_source.h"

namespace content {

namespace {

mojom::NetworkRequestStatus ConvertInterruptReasonToMojoNetworkRequestStatus(
    DownloadInterruptReason reason) {
  switch (reason) {
    case DOWNLOAD_INTERRUPT_REASON_NONE:
      return mojom::NetworkRequestStatus::OK;
    case DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT:
      return mojom::NetworkRequestStatus::NETWORK_TIMEOUT;
    case DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED:
      return mojom::NetworkRequestStatus::NETWORK_DISCONNECTED;
    case DOWNLOAD_INTERRUPT_REASON_NETWORK_SERVER_DOWN:
      return mojom::NetworkRequestStatus::NETWORK_SERVER_DOWN;
    case DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE:
      return mojom::NetworkRequestStatus::SERVER_NO_RANGE;
    case DOWNLOAD_INTERRUPT_REASON_SERVER_CONTENT_LENGTH_MISMATCH:
      return mojom::NetworkRequestStatus::SERVER_CONTENT_LENGTH_MISMATCH;
    case DOWNLOAD_INTERRUPT_REASON_SERVER_UNREACHABLE:
      return mojom::NetworkRequestStatus::SERVER_UNREACHABLE;
    case DOWNLOAD_INTERRUPT_REASON_SERVER_CERT_PROBLEM:
      return mojom::NetworkRequestStatus::SERVER_CERT_PROBLEM;
    case DOWNLOAD_INTERRUPT_REASON_USER_CANCELED:
      return mojom::NetworkRequestStatus::USER_CANCELED;
    case DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED:
      return mojom::NetworkRequestStatus::NETWORK_FAILED;
    default:
      NOTREACHED();
      return mojom::NetworkRequestStatus::NETWORK_FAILED;
  }
}

}  // namespace

DownloadResponseHandler::DownloadResponseHandler(
    ResourceRequest* resource_request,
    Delegate* delegate,
    std::unique_ptr<DownloadSaveInfo> save_info,
    bool is_parallel_request,
    bool is_transient)
    : delegate_(delegate),
      started_(false),
      save_info_(std::move(save_info)),
      url_chain_(1, resource_request->url),
      method_(resource_request->method),
      referrer_(resource_request->referrer),
      is_transient_(is_transient),
      has_strong_validators_(false) {
  if (!is_parallel_request)
    RecordDownloadCount(UNTHROTTLED_COUNT);
  if (resource_request->request_initiator.has_value())
    origin_ = resource_request->request_initiator.value().GetURL();
}

DownloadResponseHandler::~DownloadResponseHandler() = default;

void DownloadResponseHandler::OnReceiveResponse(
    const ResourceResponseHead& head,
    const base::Optional<net::SSLInfo>& ssl_info,
    mojom::DownloadedTempFilePtr downloaded_file) {
  create_info_ = CreateDownloadCreateInfo(head);

  if (ssl_info)
    cert_status_ = ssl_info->cert_status;

  // TODO(xingliu): Do not use http cache.
  // Sets page transition type correctly and call
  // |RecordDownloadSourcePageTransitionType| here.
  if (head.headers) {
    has_strong_validators_ = head.headers->HasStrongValidators();
    RecordDownloadHttpResponseCode(head.headers->response_code());
    RecordDownloadContentDisposition(create_info_->content_disposition);
  }

  // Blink verifies that the requester of this download is allowed to set a
  // suggested name for the security origin of the downlaod URL. However, this
  // assumption doesn't hold if there were cross origin redirects. Therefore,
  // clear the suggested_name for such requests.
  if (origin_.is_valid() && !create_info_->url_chain.back().SchemeIsBlob() &&
      !create_info_->url_chain.back().SchemeIs(url::kAboutScheme) &&
      !create_info_->url_chain.back().SchemeIs(url::kDataScheme) &&
      origin_ != create_info_->url_chain.back().GetOrigin()) {
    create_info_->save_info->suggested_name.clear();
  }

  if (create_info_->result != DOWNLOAD_INTERRUPT_REASON_NONE)
    OnResponseStarted(mojom::DownloadStreamHandlePtr());
}

std::unique_ptr<DownloadCreateInfo>
DownloadResponseHandler::CreateDownloadCreateInfo(
    const ResourceResponseHead& head) {
  // TODO(qinmin): instead of using NetLogWithSource, introduce new logging
  // class for download.
  auto create_info = base::MakeUnique<DownloadCreateInfo>(
      base::Time::Now(), net::NetLogWithSource(), std::move(save_info_));

  DownloadInterruptReason result =
      head.headers ? HandleSuccessfulServerResponse(
                         *head.headers, create_info->save_info.get())
                   : DOWNLOAD_INTERRUPT_REASON_NONE;

  create_info->result = result;
  if (result == DOWNLOAD_INTERRUPT_REASON_NONE)
    create_info->remote_address = head.socket_address.host();
  create_info->method = method_;
  create_info->connection_info = head.connection_info;
  create_info->url_chain = url_chain_;
  create_info->referrer_url = referrer_;
  create_info->transient = is_transient_;
  create_info->response_headers = head.headers;
  create_info->offset = create_info->save_info->offset;
  create_info->mime_type = head.mime_type;

  HandleResponseHeaders(head.headers.get(), create_info.get());
  return create_info;
}

void DownloadResponseHandler::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const content::ResourceResponseHead& head) {
  url_chain_.push_back(redirect_info.new_url);
  method_ = redirect_info.new_method;
  referrer_ = GURL(redirect_info.new_referrer);
  delegate_->OnReceiveRedirect();
}

void DownloadResponseHandler::OnDataDownloaded(int64_t data_length,
                                               int64_t encoded_length) {}

void DownloadResponseHandler::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback callback) {}

void DownloadResponseHandler::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {}

void DownloadResponseHandler::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {}

void DownloadResponseHandler::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  if (started_)
    return;

  mojom::DownloadStreamHandlePtr stream_handle =
      mojom::DownloadStreamHandle::New();
  stream_handle->stream = std::move(body);
  stream_handle->client_request = mojo::MakeRequest(&client_ptr_);
  OnResponseStarted(std::move(stream_handle));
}

void DownloadResponseHandler::OnComplete(
    const content::ResourceRequestCompletionStatus& completion_status) {
  DownloadInterruptReason reason = HandleRequestCompletionStatus(
      static_cast<net::Error>(completion_status.error_code),
      has_strong_validators_, cert_status_, DOWNLOAD_INTERRUPT_REASON_NONE);

  if (client_ptr_) {
    client_ptr_->OnStreamCompleted(
        ConvertInterruptReasonToMojoNetworkRequestStatus(reason));
  }

  if (started_)
    return;

  // OnComplete() called without OnReceiveResponse(). This should only
  // happen when the request was aborted.
  create_info_ = CreateDownloadCreateInfo(ResourceResponseHead());
  create_info_->result = reason;
  OnResponseStarted(mojom::DownloadStreamHandlePtr());
}

void DownloadResponseHandler::OnResponseStarted(
    mojom::DownloadStreamHandlePtr stream_handle) {
  started_ = true;
  delegate_->OnResponseStarted(std::move(create_info_),
                               std::move(stream_handle));
}

}  // namespace content
