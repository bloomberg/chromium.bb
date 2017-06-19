// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_network_transaction.h"

#include <utility>

#include "base/callback_helpers.h"
#include "chrome/browser/devtools/devtools_network_controller.h"
#include "chrome/browser/devtools/devtools_network_interceptor.h"
#include "chrome/browser/devtools/devtools_network_upload_data_stream.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_errors.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_request_info.h"
#include "net/socket/connection_attempts.h"

// Keep in sync with X_DevTools_Emulate_Network_Conditions_Client_Id defined in
// HTTPNames.json5.
const char
    DevToolsNetworkTransaction::kDevToolsEmulateNetworkConditionsClientId[] =
        "X-DevTools-Emulate-Network-Conditions-Client-Id";

DevToolsNetworkTransaction::DevToolsNetworkTransaction(
    DevToolsNetworkController* controller,
    std::unique_ptr<net::HttpTransaction> network_transaction)
    : throttled_byte_count_(0),
      controller_(controller),
      network_transaction_(std::move(network_transaction)),
      request_(nullptr),
      failed_(false) {
  DCHECK(controller);
}

DevToolsNetworkTransaction::~DevToolsNetworkTransaction() {
  if (interceptor_ && !throttle_callback_.is_null())
    interceptor_->StopThrottle(throttle_callback_);
}

void DevToolsNetworkTransaction::IOCallback(
    const net::CompletionCallback& callback, bool start, int result) {
  result = Throttle(callback, start, result);
  if (result != net::ERR_IO_PENDING)
    callback.Run(result);
}

int DevToolsNetworkTransaction::Throttle(
    const net::CompletionCallback& callback, bool start, int result) {
  if (failed_)
    return net::ERR_INTERNET_DISCONNECTED;
  if (!interceptor_ || result < 0)
    return result;

  base::TimeTicks send_end;
  if (start) {
    throttled_byte_count_ += network_transaction_->GetTotalReceivedBytes();
    net::LoadTimingInfo load_timing_info;
    if (GetLoadTimingInfo(&load_timing_info)) {
      send_end = load_timing_info.send_end;
      if (!load_timing_info.push_start.is_null())
        start = false;
    }
    if (send_end.is_null())
      send_end = base::TimeTicks::Now();
  }
  if (result > 0)
    throttled_byte_count_ += result;

  throttle_callback_ = base::Bind(&DevToolsNetworkTransaction::ThrottleCallback,
      base::Unretained(this), callback);
  int rv = interceptor_->StartThrottle(result, throttled_byte_count_, send_end,
      start, false, throttle_callback_);
  if (rv != net::ERR_IO_PENDING)
    throttle_callback_.Reset();
  if (rv == net::ERR_INTERNET_DISCONNECTED)
    Fail();
  return rv;
}

void DevToolsNetworkTransaction::ThrottleCallback(
    const net::CompletionCallback& callback, int result, int64_t bytes) {
  DCHECK(!throttle_callback_.is_null());
  throttle_callback_.Reset();
  if (result == net::ERR_INTERNET_DISCONNECTED)
    Fail();
  throttled_byte_count_ = bytes;
  callback.Run(result);
}

void DevToolsNetworkTransaction::Fail() {
  DCHECK(request_);
  DCHECK(!failed_);
  failed_ = true;
  network_transaction_->SetBeforeNetworkStartCallback(
      BeforeNetworkStartCallback());
  if (interceptor_)
    interceptor_.reset();
}

bool DevToolsNetworkTransaction::CheckFailed() {
  if (failed_)
    return true;
  if (interceptor_ && interceptor_->IsOffline()) {
    Fail();
    return true;
  }
  return false;
}

int DevToolsNetworkTransaction::Start(const net::HttpRequestInfo* request,
                                      const net::CompletionCallback& callback,
                                      const net::NetLogWithSource& net_log) {
  DCHECK(request);
  request_ = request;

  std::string client_id;
  bool has_devtools_client_id = request_->extra_headers.HasHeader(
      kDevToolsEmulateNetworkConditionsClientId);
  if (has_devtools_client_id) {
    custom_request_.reset(new net::HttpRequestInfo(*request_));
    custom_request_->extra_headers.GetHeader(
        kDevToolsEmulateNetworkConditionsClientId, &client_id);
    custom_request_->extra_headers.RemoveHeader(
        kDevToolsEmulateNetworkConditionsClientId);

    if (request_->upload_data_stream) {
      custom_upload_data_stream_.reset(
          new DevToolsNetworkUploadDataStream(request_->upload_data_stream));
      custom_request_->upload_data_stream = custom_upload_data_stream_.get();
    }

    request_ = custom_request_.get();
  }

  DevToolsNetworkInterceptor* interceptor =
      controller_->GetInterceptor(client_id);
  if (interceptor) {
    interceptor_ = interceptor->GetWeakPtr();
    if (custom_upload_data_stream_)
      custom_upload_data_stream_->SetInterceptor(interceptor);
  }

  if (CheckFailed())
    return net::ERR_INTERNET_DISCONNECTED;

  if (!interceptor_)
    return network_transaction_->Start(request_, callback, net_log);

  int result = network_transaction_->Start(request_,
      base::Bind(&DevToolsNetworkTransaction::IOCallback,
                 base::Unretained(this), callback, true),
      net_log);
  return Throttle(callback, true, result);
}

int DevToolsNetworkTransaction::RestartIgnoringLastError(
    const net::CompletionCallback& callback) {
  if (CheckFailed())
    return net::ERR_INTERNET_DISCONNECTED;
  if (!interceptor_)
    return network_transaction_->RestartIgnoringLastError(callback);

  int result = network_transaction_->RestartIgnoringLastError(
      base::Bind(&DevToolsNetworkTransaction::IOCallback,
                 base::Unretained(this), callback, true));
  return Throttle(callback, true, result);
}

int DevToolsNetworkTransaction::RestartWithCertificate(
    scoped_refptr<net::X509Certificate> client_cert,
    scoped_refptr<net::SSLPrivateKey> client_private_key,
    const net::CompletionCallback& callback) {
  if (CheckFailed())
    return net::ERR_INTERNET_DISCONNECTED;
  if (!interceptor_) {
    return network_transaction_->RestartWithCertificate(
        std::move(client_cert), std::move(client_private_key), callback);
  }

  int result = network_transaction_->RestartWithCertificate(
      std::move(client_cert), std::move(client_private_key),
      base::Bind(&DevToolsNetworkTransaction::IOCallback,
                 base::Unretained(this), callback, true));
  return Throttle(callback, true, result);
}

int DevToolsNetworkTransaction::RestartWithAuth(
    const net::AuthCredentials& credentials,
    const net::CompletionCallback& callback) {
  if (CheckFailed())
    return net::ERR_INTERNET_DISCONNECTED;
  if (!interceptor_)
    return network_transaction_->RestartWithAuth(credentials, callback);

  int result = network_transaction_->RestartWithAuth(credentials,
      base::Bind(&DevToolsNetworkTransaction::IOCallback,
                 base::Unretained(this), callback, true));
  return Throttle(callback, true, result);
}

bool DevToolsNetworkTransaction::IsReadyToRestartForAuth() {
  return network_transaction_->IsReadyToRestartForAuth();
}

int DevToolsNetworkTransaction::Read(
    net::IOBuffer* buf,
    int buf_len,
    const net::CompletionCallback& callback) {
  if (CheckFailed())
    return net::ERR_INTERNET_DISCONNECTED;
  if (!interceptor_)
    return network_transaction_->Read(buf, buf_len, callback);

  int result = network_transaction_->Read(buf, buf_len,
      base::Bind(&DevToolsNetworkTransaction::IOCallback,
                 base::Unretained(this), callback, false));
  // URLRequestJob relies on synchronous end-of-stream notification.
  if (result == 0)
    return result;
  return Throttle(callback, false, result);
}

void DevToolsNetworkTransaction::StopCaching() {
  network_transaction_->StopCaching();
}

bool DevToolsNetworkTransaction::GetFullRequestHeaders(
    net::HttpRequestHeaders* headers) const {
  return network_transaction_->GetFullRequestHeaders(headers);
}

int64_t DevToolsNetworkTransaction::GetTotalReceivedBytes() const {
  return network_transaction_->GetTotalReceivedBytes();
}

int64_t DevToolsNetworkTransaction::GetTotalSentBytes() const {
  return network_transaction_->GetTotalSentBytes();
}

void DevToolsNetworkTransaction::DoneReading() {
  network_transaction_->DoneReading();
}

const net::HttpResponseInfo*
DevToolsNetworkTransaction::GetResponseInfo() const {
  return network_transaction_->GetResponseInfo();
}

net::LoadState DevToolsNetworkTransaction::GetLoadState() const {
  return network_transaction_->GetLoadState();
}

void DevToolsNetworkTransaction::SetQuicServerInfo(
    net::QuicServerInfo* quic_server_info) {
  network_transaction_->SetQuicServerInfo(quic_server_info);
}

bool DevToolsNetworkTransaction::GetLoadTimingInfo(
    net::LoadTimingInfo* load_timing_info) const {
  return network_transaction_->GetLoadTimingInfo(load_timing_info);
}

bool DevToolsNetworkTransaction::GetRemoteEndpoint(
    net::IPEndPoint* endpoint) const {
  return network_transaction_->GetRemoteEndpoint(endpoint);
}

void DevToolsNetworkTransaction::PopulateNetErrorDetails(
    net::NetErrorDetails* details) const {
  return network_transaction_->PopulateNetErrorDetails(details);
}

void DevToolsNetworkTransaction::SetPriority(net::RequestPriority priority) {
  network_transaction_->SetPriority(priority);
}

void DevToolsNetworkTransaction::SetWebSocketHandshakeStreamCreateHelper(
    net::WebSocketHandshakeStreamBase::CreateHelper* create_helper) {
  network_transaction_->SetWebSocketHandshakeStreamCreateHelper(create_helper);
}

void DevToolsNetworkTransaction::SetBeforeNetworkStartCallback(
    const BeforeNetworkStartCallback& callback) {
  network_transaction_->SetBeforeNetworkStartCallback(callback);
}

void DevToolsNetworkTransaction::SetBeforeHeadersSentCallback(
    const BeforeHeadersSentCallback& callback) {
  network_transaction_->SetBeforeHeadersSentCallback(callback);
}

int DevToolsNetworkTransaction::ResumeNetworkStart() {
  if (CheckFailed())
    return net::ERR_INTERNET_DISCONNECTED;
  return network_transaction_->ResumeNetworkStart();
}

void
DevToolsNetworkTransaction::GetConnectionAttempts(net::ConnectionAttempts* out)
const {
  network_transaction_->GetConnectionAttempts(out);
}
