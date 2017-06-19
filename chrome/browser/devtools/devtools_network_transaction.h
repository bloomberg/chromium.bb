// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_TRANSACTION_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_TRANSACTION_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/devtools/devtools_network_interceptor.h"
#include "net/base/completion_callback.h"
#include "net/base/load_states.h"
#include "net/base/net_error_details.h"
#include "net/base/request_priority.h"
#include "net/http/http_transaction.h"
#include "net/websockets/websocket_handshake_stream_base.h"

class DevToolsNetworkController;
class DevToolsNetworkUploadDataStream;

namespace net {
class AuthCredentials;
class HttpRequestHeaders;
struct HttpRequestInfo;
class HttpResponseInfo;
class IOBuffer;
struct LoadTimingInfo;
class NetLogWithSource;
class X509Certificate;
}  // namespace net

namespace test {
class DevToolsNetworkControllerHelper;
}

// DevToolsNetworkTransaction is a wrapper for network transaction. All
// HttpTransaction methods are proxied to real transaction, but |callback|
// parameter is saved and replaced with proxy callback. Fail method should be
// used to simulate network outage. It runs saved callback (if any) with
// net::ERR_INTERNET_DISCONNECTED result value.
class DevToolsNetworkTransaction
    : public net::HttpTransaction {
 public:
  static const char kDevToolsEmulateNetworkConditionsClientId[];

  DevToolsNetworkTransaction(
      DevToolsNetworkController* controller,
      std::unique_ptr<net::HttpTransaction> network_transaction);

  ~DevToolsNetworkTransaction() override;

  // HttpTransaction methods:
  int Start(const net::HttpRequestInfo* request,
            const net::CompletionCallback& callback,
            const net::NetLogWithSource& net_log) override;
  int RestartIgnoringLastError(
      const net::CompletionCallback& callback) override;
  int RestartWithCertificate(
      scoped_refptr<net::X509Certificate> client_cert,
      scoped_refptr<net::SSLPrivateKey> client_private_key,
      const net::CompletionCallback& callback) override;
  int RestartWithAuth(const net::AuthCredentials& credentials,
                      const net::CompletionCallback& callback) override;
  bool IsReadyToRestartForAuth() override;

  int Read(net::IOBuffer* buf,
           int buf_len,
           const net::CompletionCallback& callback) override;
  void StopCaching() override;
  bool GetFullRequestHeaders(net::HttpRequestHeaders* headers) const override;
  int64_t GetTotalReceivedBytes() const override;
  int64_t GetTotalSentBytes() const override;
  void DoneReading() override;
  const net::HttpResponseInfo* GetResponseInfo() const override;
  net::LoadState GetLoadState() const override;
  void SetQuicServerInfo(net::QuicServerInfo* quic_server_info) override;
  bool GetLoadTimingInfo(net::LoadTimingInfo* load_timing_info) const override;
  bool GetRemoteEndpoint(net::IPEndPoint* endpoint) const override;
  void PopulateNetErrorDetails(net::NetErrorDetails* details) const override;
  void SetPriority(net::RequestPriority priority) override;
  void SetWebSocketHandshakeStreamCreateHelper(
      net::WebSocketHandshakeStreamBase::CreateHelper* create_helper) override;
  void SetBeforeNetworkStartCallback(
      const BeforeNetworkStartCallback& callback) override;
  void SetBeforeHeadersSentCallback(
      const BeforeHeadersSentCallback& callback) override;
  int ResumeNetworkStart() override;
  void GetConnectionAttempts(net::ConnectionAttempts* out) const override;

 protected:
  friend class test::DevToolsNetworkControllerHelper;

 private:
  void Fail();
  bool CheckFailed();

  void IOCallback(const net::CompletionCallback& callback,
                  bool start,
                  int result);
  int Throttle(const net::CompletionCallback& callback,
               bool start,
               int result);
  void ThrottleCallback(const net::CompletionCallback& callback,
                        int result,
                        int64_t bytes);

  DevToolsNetworkInterceptor::ThrottleCallback throttle_callback_;
  int64_t throttled_byte_count_;

  DevToolsNetworkController* controller_;
  base::WeakPtr<DevToolsNetworkInterceptor> interceptor_;

  // Modified upload data stream. Should be destructed after |custom_request_|.
  std::unique_ptr<DevToolsNetworkUploadDataStream> custom_upload_data_stream_;

  // Modified request. Should be destructed after |network_transaction_|.
  std::unique_ptr<net::HttpRequestInfo> custom_request_;

  // Real network transaction.
  std::unique_ptr<net::HttpTransaction> network_transaction_;

  const net::HttpRequestInfo* request_;

  // True if Fail was already invoked.
  bool failed_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsNetworkTransaction);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_TRANSACTION_H_
