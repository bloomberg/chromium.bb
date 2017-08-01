// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_update_url_loader_request.h"
#include "content/browser/appcache/appcache_update_url_fetcher.h"
#include "net/url_request/url_request_context.h"

namespace content {

AppCacheUpdateJob::UpdateURLLoaderRequest::~UpdateURLLoaderRequest() {}

void AppCacheUpdateJob::UpdateURLLoaderRequest::Start() {
  NOTIMPLEMENTED();
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  NOTIMPLEMENTED();
}

GURL AppCacheUpdateJob::UpdateURLLoaderRequest::GetURL() const {
  NOTIMPLEMENTED();
  return GURL();
}

GURL AppCacheUpdateJob::UpdateURLLoaderRequest::GetOriginalURL() const {
  NOTIMPLEMENTED();
  return GURL();
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::SetLoadFlags(int flags) {
  NOTIMPLEMENTED();
}

int AppCacheUpdateJob::UpdateURLLoaderRequest::GetLoadFlags() const {
  NOTIMPLEMENTED();
  return 0;
}

std::string AppCacheUpdateJob::UpdateURLLoaderRequest::GetMimeType() const {
  NOTIMPLEMENTED();
  return std::string();
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::SetFirstPartyForCookies(
    const GURL& first_party_for_cookies) {
  NOTIMPLEMENTED();
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::SetInitiator(
    const base::Optional<url::Origin>& initiator) {
  NOTIMPLEMENTED();
}

net::HttpResponseHeaders*
AppCacheUpdateJob::UpdateURLLoaderRequest::GetResponseHeaders() const {
  NOTIMPLEMENTED();
  return nullptr;
}

int AppCacheUpdateJob::UpdateURLLoaderRequest::GetResponseCode() const {
  NOTIMPLEMENTED();
  return 0;
}

net::HttpResponseInfo
AppCacheUpdateJob::UpdateURLLoaderRequest::GetResponseInfo() const {
  NOTIMPLEMENTED();
  return net::HttpResponseInfo();
}

const net::URLRequestContext*
AppCacheUpdateJob::UpdateURLLoaderRequest::GetRequestContext() const {
  NOTIMPLEMENTED();
  return nullptr;
}

int AppCacheUpdateJob::UpdateURLLoaderRequest::Read(net::IOBuffer* buf,
                                                    int max_bytes) {
  NOTIMPLEMENTED();
  return 0;
}

int AppCacheUpdateJob::UpdateURLLoaderRequest::Cancel() {
  NOTIMPLEMENTED();
  return 0;
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::OnReceiveResponse(
    const ResourceResponseHead& response_head,
    const base::Optional<net::SSLInfo>& ssl_info,
    mojom::DownloadedTempFilePtr downloaded_file) {
  NOTIMPLEMENTED();
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const ResourceResponseHead& response_head) {
  NOTIMPLEMENTED();
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::OnDataDownloaded(
    int64_t data_len,
    int64_t encoded_data_len) {
  NOTIMPLEMENTED();
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  NOTIMPLEMENTED();
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {
  NOTIMPLEMENTED();
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  NOTIMPLEMENTED();
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  NOTIMPLEMENTED();
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::OnComplete(
    const ResourceRequestCompletionStatus& status) {
  NOTIMPLEMENTED();
}

AppCacheUpdateJob::UpdateURLLoaderRequest::UpdateURLLoaderRequest(
    net::URLRequestContext* request_context,
    const GURL& url,
    URLFetcher* fetcher) {}

}  // namespace content
