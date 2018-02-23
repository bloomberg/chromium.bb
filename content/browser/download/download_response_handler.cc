// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_response_handler.h"

#include <memory>

#include "components/download/public/common/download_stats.h"
#include "components/download/public/common/download_url_parameters.h"
#include "content/browser/download/download_utils.h"
#include "net/http/http_status_code.h"
#include "net/log/net_log_with_source.h"

namespace content {

namespace {

download::mojom::NetworkRequestStatus
ConvertInterruptReasonToMojoNetworkRequestStatus(
    download::DownloadInterruptReason reason) {
  switch (reason) {
    case download::DOWNLOAD_INTERRUPT_REASON_NONE:
      return download::mojom::NetworkRequestStatus::OK;
    case download::DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT:
      return download::mojom::NetworkRequestStatus::NETWORK_TIMEOUT;
    case download::DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED:
      return download::mojom::NetworkRequestStatus::NETWORK_DISCONNECTED;
    case download::DOWNLOAD_INTERRUPT_REASON_NETWORK_SERVER_DOWN:
      return download::mojom::NetworkRequestStatus::NETWORK_SERVER_DOWN;
    case download::DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE:
      return download::mojom::NetworkRequestStatus::SERVER_NO_RANGE;
    case download::DOWNLOAD_INTERRUPT_REASON_SERVER_CONTENT_LENGTH_MISMATCH:
      return download::mojom::NetworkRequestStatus::
          SERVER_CONTENT_LENGTH_MISMATCH;
    case download::DOWNLOAD_INTERRUPT_REASON_SERVER_UNREACHABLE:
      return download::mojom::NetworkRequestStatus::SERVER_UNREACHABLE;
    case download::DOWNLOAD_INTERRUPT_REASON_SERVER_CERT_PROBLEM:
      return download::mojom::NetworkRequestStatus::SERVER_CERT_PROBLEM;
    case download::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED:
      return download::mojom::NetworkRequestStatus::USER_CANCELED;
    case download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED:
      return download::mojom::NetworkRequestStatus::NETWORK_FAILED;
    default:
      NOTREACHED();
      return download::mojom::NetworkRequestStatus::NETWORK_FAILED;
  }
}

}  // namespace

DownloadResponseHandler::DownloadResponseHandler(
    network::ResourceRequest* resource_request,
    Delegate* delegate,
    std::unique_ptr<download::DownloadSaveInfo> save_info,
    bool is_parallel_request,
    bool is_transient,
    bool fetch_error_body,
    const std::string& request_origin,
    download::DownloadSource download_source,
    std::vector<GURL> url_chain)
    : delegate_(delegate),
      started_(false),
      save_info_(std::move(save_info)),
      url_chain_(std::move(url_chain)),
      method_(resource_request->method),
      referrer_(resource_request->referrer),
      is_transient_(is_transient),
      fetch_error_body_(fetch_error_body),
      request_origin_(request_origin),
      download_source_(download_source),
      has_strong_validators_(false),
      is_partial_request_(save_info_->offset > 0),
      abort_reason_(download::DOWNLOAD_INTERRUPT_REASON_NONE) {
  if (!is_parallel_request) {
    download::RecordDownloadCountWithSource(download::UNTHROTTLED_COUNT,
                                            download_source);
  }
  if (resource_request->request_initiator.has_value())
    origin_ = resource_request->request_initiator.value().GetURL();
}

DownloadResponseHandler::~DownloadResponseHandler() = default;

void DownloadResponseHandler::OnReceiveResponse(
    const network::ResourceResponseHead& head,
    const base::Optional<net::SSLInfo>& ssl_info,
    network::mojom::DownloadedTempFilePtr downloaded_file) {
  create_info_ = CreateDownloadCreateInfo(head);

  if (ssl_info)
    cert_status_ = ssl_info->cert_status;

  // TODO(xingliu): Do not use http cache.
  // Sets page transition type correctly and call
  // |RecordDownloadSourcePageTransitionType| here.
  if (head.headers) {
    has_strong_validators_ = head.headers->HasStrongValidators();
    download::RecordDownloadHttpResponseCode(head.headers->response_code());
    download::RecordDownloadContentDisposition(
        create_info_->content_disposition);
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

  if (create_info_->result != download::DOWNLOAD_INTERRUPT_REASON_NONE)
    OnResponseStarted(download::mojom::DownloadStreamHandlePtr());
}

std::unique_ptr<download::DownloadCreateInfo>
DownloadResponseHandler::CreateDownloadCreateInfo(
    const network::ResourceResponseHead& head) {
  // TODO(qinmin): instead of using NetLogWithSource, introduce new logging
  // class for download.
  auto create_info = std::make_unique<download::DownloadCreateInfo>(
      base::Time::Now(), std::move(save_info_));

  download::DownloadInterruptReason result =
      head.headers
          ? HandleSuccessfulServerResponse(
                *head.headers, create_info->save_info.get(), fetch_error_body_)
          : download::DOWNLOAD_INTERRUPT_REASON_NONE;

  create_info->total_bytes = head.content_length > 0 ? head.content_length : 0;
  create_info->result = result;
  if (result == download::DOWNLOAD_INTERRUPT_REASON_NONE)
    create_info->remote_address = head.socket_address.host();
  create_info->method = method_;
  create_info->connection_info = head.connection_info;
  create_info->url_chain = url_chain_;
  create_info->referrer_url = referrer_;
  create_info->transient = is_transient_;
  create_info->response_headers = head.headers;
  create_info->offset = create_info->save_info->offset;
  create_info->mime_type = head.mime_type;
  create_info->fetch_error_body = fetch_error_body_;
  create_info->request_origin = request_origin_;
  create_info->download_source = download_source_;

  HandleResponseHeaders(head.headers.get(), create_info.get());
  return create_info;
}

void DownloadResponseHandler::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& head) {
  if (is_partial_request_) {
    // A redirect while attempting a partial resumption indicates a potential
    // middle box. Trigger another interruption so that the
    // download::DownloadItem can retry.
    abort_reason_ = download::DOWNLOAD_INTERRUPT_REASON_SERVER_UNREACHABLE;
    OnComplete(network::URLLoaderCompletionStatus(net::OK));
    return;
  }
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

  download::mojom::DownloadStreamHandlePtr stream_handle =
      download::mojom::DownloadStreamHandle::New();
  stream_handle->stream = std::move(body);
  stream_handle->client_request = mojo::MakeRequest(&client_ptr_);
  OnResponseStarted(std::move(stream_handle));
}

void DownloadResponseHandler::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  download::DownloadInterruptReason reason = HandleRequestCompletionStatus(
      static_cast<net::Error>(status.error_code), has_strong_validators_,
      cert_status_, abort_reason_);

  if (client_ptr_) {
    client_ptr_->OnStreamCompleted(
        ConvertInterruptReasonToMojoNetworkRequestStatus(reason));
  }

  if (started_)
    return;

  // OnComplete() called without OnReceiveResponse(). This should only
  // happen when the request was aborted.
  create_info_ = CreateDownloadCreateInfo(network::ResourceResponseHead());
  create_info_->result = reason;

  OnResponseStarted(download::mojom::DownloadStreamHandlePtr());
}

void DownloadResponseHandler::OnResponseStarted(
    download::mojom::DownloadStreamHandlePtr stream_handle) {
  started_ = true;
  delegate_->OnResponseStarted(std::move(create_info_),
                               std::move(stream_handle));
}

}  // namespace content
