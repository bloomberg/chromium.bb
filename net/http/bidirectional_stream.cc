// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/bidirectional_stream.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/bidirectional_stream_request_info.h"
#include "net/http/http_network_session.h"
#include "net/http/http_stream.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_config.h"
#include "url/gurl.h"

namespace net {

BidirectionalStream::Delegate::Delegate() {}

BidirectionalStream::Delegate::~Delegate() {}

BidirectionalStream::BidirectionalStream(
    scoped_ptr<BidirectionalStreamRequestInfo> request_info,
    HttpNetworkSession* session,
    Delegate* delegate)
    : BidirectionalStream(std::move(request_info),
                          session,
                          delegate,
                          make_scoped_ptr(new base::Timer(false, false))) {}

BidirectionalStream::BidirectionalStream(
    scoped_ptr<BidirectionalStreamRequestInfo> request_info,
    HttpNetworkSession* session,
    Delegate* delegate,
    scoped_ptr<base::Timer> timer)
    : request_info_(std::move(request_info)),
      net_log_(BoundNetLog::Make(session->net_log(),
                                 NetLog::SOURCE_BIDIRECTIONAL_STREAM)),
      delegate_(delegate),
      timer_(std::move(timer)) {
  DCHECK(delegate_);
  DCHECK(request_info_);

  SSLConfig server_ssl_config;
  session->ssl_config_service()->GetSSLConfig(&server_ssl_config);
  session->GetAlpnProtos(&server_ssl_config.alpn_protos);
  session->GetNpnProtos(&server_ssl_config.npn_protos);

  if (!request_info_->url.SchemeIs(url::kHttpsScheme)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&BidirectionalStream::Delegate::OnFailed,
                   base::Unretained(delegate_), ERR_DISALLOWED_URL_SCHEME));
    return;
  }

  HttpRequestInfo http_request_info;
  http_request_info.url = request_info_->url;
  http_request_info.method = request_info_->method;
  http_request_info.extra_headers = request_info_->extra_headers;
  stream_request_.reset(
      session->http_stream_factory()->RequestBidirectionalStreamJob(
          http_request_info, request_info_->priority, server_ssl_config,
          server_ssl_config, this, net_log_));
  // Check that this call cannot fail to set a non-NULL |stream_request_|.
  DCHECK(stream_request_);
  // Check that HttpStreamFactory does not invoke OnBidirectionalStreamJobReady
  // synchronously.
  DCHECK(!stream_job_);
}

BidirectionalStream::~BidirectionalStream() {
  Cancel();
}

int BidirectionalStream::ReadData(IOBuffer* buf, int buf_len) {
  DCHECK(stream_job_);

  return stream_job_->ReadData(buf, buf_len);
}

void BidirectionalStream::SendData(IOBuffer* data,
                                   int length,
                                   bool end_stream) {
  DCHECK(stream_job_);

  stream_job_->SendData(data, length, end_stream);
}

void BidirectionalStream::Cancel() {
  stream_request_.reset();
  if (stream_job_) {
    stream_job_->Cancel();
    stream_job_.reset();
  }
}

NextProto BidirectionalStream::GetProtocol() const {
  if (!stream_job_)
    return kProtoUnknown;

  return stream_job_->GetProtocol();
}

int64_t BidirectionalStream::GetTotalReceivedBytes() const {
  if (!stream_job_)
    return 0;

  return stream_job_->GetTotalReceivedBytes();
}

int64_t BidirectionalStream::GetTotalSentBytes() const {
  if (!stream_job_)
    return 0;

  return stream_job_->GetTotalSentBytes();
}

void BidirectionalStream::OnHeadersSent() {
  delegate_->OnHeadersSent();
}

void BidirectionalStream::OnHeadersReceived(
    const SpdyHeaderBlock& response_headers) {
  delegate_->OnHeadersReceived(response_headers);
}

void BidirectionalStream::OnDataRead(int bytes_read) {
  delegate_->OnDataRead(bytes_read);
}

void BidirectionalStream::OnDataSent() {
  delegate_->OnDataSent();
}

void BidirectionalStream::OnTrailersReceived(const SpdyHeaderBlock& trailers) {
  delegate_->OnTrailersReceived(trailers);
}

void BidirectionalStream::OnFailed(int status) {
  delegate_->OnFailed(status);
}

void BidirectionalStream::OnStreamReady(const SSLConfig& used_ssl_config,
                                        const ProxyInfo& used_proxy_info,
                                        HttpStream* stream) {
  NOTREACHED();
}

void BidirectionalStream::OnBidirectionalStreamJobReady(
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    BidirectionalStreamJob* stream) {
  DCHECK(!stream_job_);

  stream_request_.reset();
  stream_job_.reset(stream);
  stream_job_->Start(request_info_.get(), net_log_, this, std::move(timer_));
}

void BidirectionalStream::OnWebSocketHandshakeStreamReady(
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    WebSocketHandshakeStreamBase* stream) {
  NOTREACHED();
}

void BidirectionalStream::OnStreamFailed(int result,
                                         const SSLConfig& used_ssl_config,
                                         SSLFailureState ssl_failure_state) {
  DCHECK_LT(result, 0);
  DCHECK_NE(result, ERR_IO_PENDING);
  DCHECK(stream_request_);

  delegate_->OnFailed(result);
}

void BidirectionalStream::OnCertificateError(int result,
                                             const SSLConfig& used_ssl_config,
                                             const SSLInfo& ssl_info) {
  DCHECK_LT(result, 0);
  DCHECK_NE(result, ERR_IO_PENDING);
  DCHECK(stream_request_);

  delegate_->OnFailed(result);
}

void BidirectionalStream::OnNeedsProxyAuth(
    const HttpResponseInfo& proxy_response,
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    HttpAuthController* auth_controller) {
  DCHECK(stream_request_);

  delegate_->OnFailed(ERR_PROXY_AUTH_REQUESTED);
}

void BidirectionalStream::OnNeedsClientAuth(const SSLConfig& used_ssl_config,
                                            SSLCertRequestInfo* cert_info) {
  DCHECK(stream_request_);

  delegate_->OnFailed(ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
}

void BidirectionalStream::OnHttpsProxyTunnelResponse(
    const HttpResponseInfo& response_info,
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    HttpStream* stream) {
  DCHECK(stream_request_);

  delegate_->OnFailed(ERR_HTTPS_PROXY_TUNNEL_RESPONSE);
}

void BidirectionalStream::OnQuicBroken() {}

}  // namespace net
