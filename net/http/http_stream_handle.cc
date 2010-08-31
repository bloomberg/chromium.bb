// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_handle.h"

namespace net {

HttpStreamHandle::HttpStreamHandle(ClientSocketHandle* connection,
                                   HttpStream* stream)
    : connection_(connection), stream_(stream) {
  DCHECK(stream_.get());
}

HttpStreamHandle::~HttpStreamHandle() {
}

int HttpStreamHandle::InitializeStream(const HttpRequestInfo* request_info,
                                       const BoundNetLog& net_log,
                                       CompletionCallback* callback) {
  return stream_->InitializeStream(request_info, net_log, callback);
}

int HttpStreamHandle::SendRequest(const std::string& request_headers,
                                  UploadDataStream* request_body,
                                  HttpResponseInfo* response,
                                  CompletionCallback* callback) {
  return stream_->SendRequest(request_headers, request_body, response,
                              callback);
}

uint64 HttpStreamHandle::GetUploadProgress() const {
  return stream_->GetUploadProgress();
}

int HttpStreamHandle::ReadResponseHeaders(CompletionCallback* callback) {
  return stream_->ReadResponseHeaders(callback);
}

const HttpResponseInfo* HttpStreamHandle::GetResponseInfo() const {
  return stream_->GetResponseInfo();
}

int HttpStreamHandle::ReadResponseBody(IOBuffer* buf, int buf_len,
                                       CompletionCallback* callback) {
  return stream_->ReadResponseBody(buf, buf_len, callback);
}

void HttpStreamHandle::Close(bool not_reusable) {
  stream_->Close(not_reusable);
}

bool HttpStreamHandle::IsResponseBodyComplete() const {
  return stream_->IsResponseBodyComplete();
}

bool HttpStreamHandle::CanFindEndOfResponse() const {
  return stream_->CanFindEndOfResponse();
}

bool HttpStreamHandle::IsMoreDataBuffered() const {
  return stream_->IsMoreDataBuffered();
}

bool HttpStreamHandle::IsConnectionReused() const {
  return stream_->IsConnectionReused();
}

void HttpStreamHandle::SetConnectionReused() {
  stream_->SetConnectionReused();
}

void HttpStreamHandle::GetSSLInfo(SSLInfo* ssl_info) {
  stream_->GetSSLInfo(ssl_info);
}

void HttpStreamHandle::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {
  stream_->GetSSLCertRequestInfo(cert_request_info);
}

}  // namespace net
