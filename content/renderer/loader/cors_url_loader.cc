// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/cors_url_loader.h"

#include "content/public/common/origin_util.h"
#include "content/public/common/service_worker_modes.h"
#include "third_party/WebKit/public/platform/WebCORS.h"
#include "third_party/WebKit/public/platform/WebHTTPHeaderMap.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"

using network::mojom::CORSError;
using network::mojom::FetchRequestMode;

namespace content {

namespace {

bool CalculateCORSFlag(const ResourceRequest& request) {
  if (request.fetch_request_mode == FetchRequestMode::kNavigate &&
      (request.resource_type == RESOURCE_TYPE_MAIN_FRAME ||
       request.resource_type == RESOURCE_TYPE_SUB_FRAME)) {
    return false;
  }
  url::Origin url_origin = url::Origin::Create(request.url);
  if (IsOriginWhiteListedTrustworthy(url_origin))
    return false;
  if (!request.request_initiator.has_value())
    return true;
  url::Origin security_origin(request.request_initiator.value());
  return !security_origin.IsSameOriginWith(url_origin);
}

}  // namespace

// TODO(toyoshim): At the moment this class simply forwards all calls to the
// underlying network loader. Part of the effort to move CORS handling out of
// Blink: http://crbug/736308.
CORSURLLoader::CORSURLLoader(
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& resource_request,
    mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojom::URLLoaderFactory* network_loader_factory)
    : network_loader_factory_(network_loader_factory),
      network_client_binding_(this),
      forwarding_client_(std::move(client)),
      security_origin_(
          resource_request.request_initiator.value_or(url::Origin())),
      last_response_url_(resource_request.url),
      fetch_request_mode_(resource_request.fetch_request_mode),
      fetch_credentials_mode_(resource_request.fetch_credentials_mode),
      fetch_cors_flag_(CalculateCORSFlag(resource_request)) {
  DCHECK(network_loader_factory_);

  if (fetch_cors_flag_ &&
      fetch_request_mode_ == FetchRequestMode::kSameOrigin) {
    forwarding_client_->OnComplete(network::URLLoaderCompletionStatus(
        network::CORSErrorStatus(CORSError::kDisallowedByMode)));
    return;
  }

  // TODO(toyoshim): Needs some checks if the calculated fetch_cors_flag_
  // is allowed in this request or not.

  mojom::URLLoaderClientPtr network_client;
  network_client_binding_.Bind(mojo::MakeRequest(&network_client));
  // Binding |this| as an unretained pointer is safe because
  // |network_client_binding_| shares this object's lifetime.
  network_client_binding_.set_connection_error_handler(base::BindOnce(
      &CORSURLLoader::OnUpstreamConnectionError, base::Unretained(this)));
  network_loader_factory_->CreateLoaderAndStart(
      mojo::MakeRequest(&network_loader_), routing_id, request_id, options,
      resource_request, std::move(network_client), traffic_annotation);
}

CORSURLLoader::~CORSURLLoader() {}

void CORSURLLoader::FollowRedirect() {
  DCHECK(network_loader_);
  DCHECK(is_waiting_follow_redirect_call_);
  is_waiting_follow_redirect_call_ = false;
  network_loader_->FollowRedirect();
}

void CORSURLLoader::ProceedWithResponse() {
  NOTREACHED();
}

void CORSURLLoader::SetPriority(net::RequestPriority priority,
                                int32_t intra_priority_value) {
  // TODO(toyoshim): Should not be called during the redirect decisions, but
  // Blink calls actually.
  // DCHECK(!is_waiting_follow_redirect_call_);
  if (network_loader_)
    network_loader_->SetPriority(priority, intra_priority_value);
}

void CORSURLLoader::PauseReadingBodyFromNet() {
  DCHECK(!is_waiting_follow_redirect_call_);
  if (network_loader_)
    network_loader_->PauseReadingBodyFromNet();
}

void CORSURLLoader::ResumeReadingBodyFromNet() {
  DCHECK(!is_waiting_follow_redirect_call_);
  if (network_loader_)
    network_loader_->ResumeReadingBodyFromNet();
}

void CORSURLLoader::OnReceiveResponse(
    const ResourceResponseHead& response_head,
    const base::Optional<net::SSLInfo>& ssl_info,
    mojom::DownloadedTempFilePtr downloaded_file) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!is_waiting_follow_redirect_call_);
  if (fetch_cors_flag_ &&
      blink::WebCORS::IsCORSEnabledRequestMode(fetch_request_mode_)) {
    base::Optional<CORSError> cors_error = blink::WebCORS::CheckAccess(
        last_response_url_, response_head.headers->response_code(),
        blink::WebHTTPHeaderMap(response_head.headers.get()),
        fetch_credentials_mode_, security_origin_);
    if (cors_error) {
      // TODO(toyoshim): Generate related_response_headers here.
      network::CORSErrorStatus cors_error_status(*cors_error);
      HandleComplete(network::URLLoaderCompletionStatus(cors_error_status));
      return;
    }
  }
  forwarding_client_->OnReceiveResponse(response_head, ssl_info,
                                        std::move(downloaded_file));
}

void CORSURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const ResourceResponseHead& response_head) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!is_waiting_follow_redirect_call_);

  // TODO(toyoshim): Following code expects OnReceivedRedirect is invoked
  // asynchronously, and |last_response_url_| and other methods should not be
  // accessed until FollowRedirect() is called.
  // We need to ensure callback behaviors once redirect implementation in this
  // class is ready for testing.
  is_waiting_follow_redirect_call_ = true;
  last_response_url_ = redirect_info.new_url;
  forwarding_client_->OnReceiveRedirect(redirect_info, response_head);
}

void CORSURLLoader::OnDataDownloaded(int64_t data_len,
                                     int64_t encoded_data_len) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!is_waiting_follow_redirect_call_);
  forwarding_client_->OnDataDownloaded(data_len, encoded_data_len);
}

void CORSURLLoader::OnUploadProgress(int64_t current_position,
                                     int64_t total_size,
                                     OnUploadProgressCallback ack_callback) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!is_waiting_follow_redirect_call_);
  forwarding_client_->OnUploadProgress(current_position, total_size,
                                       std::move(ack_callback));
}

void CORSURLLoader::OnReceiveCachedMetadata(const std::vector<uint8_t>& data) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!is_waiting_follow_redirect_call_);
  forwarding_client_->OnReceiveCachedMetadata(data);
}

void CORSURLLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!is_waiting_follow_redirect_call_);
  forwarding_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void CORSURLLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!is_waiting_follow_redirect_call_);
  forwarding_client_->OnStartLoadingResponseBody(std::move(body));
}

void CORSURLLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!is_waiting_follow_redirect_call_);
  HandleComplete(status);
}

void CORSURLLoader::OnUpstreamConnectionError() {
  // |network_client_binding_| has experienced a connection error and will no
  // longer call any of the mojom::URLLoaderClient methods above. The client
  // pipe to the downstream client is closed to inform it of this failure. The
  // client should respond by closing its mojom::URLLoader pipe which will cause
  // this object to be destroyed.
  forwarding_client_.reset();
}

void CORSURLLoader::HandleComplete(
    const network::URLLoaderCompletionStatus& status) {
  forwarding_client_->OnComplete(status);
  forwarding_client_.reset();

  // Close pipes to ignore possible subsequent callback invocations.
  network_client_binding_.Close();
  network_loader_.reset();
}

}  // namespace content
