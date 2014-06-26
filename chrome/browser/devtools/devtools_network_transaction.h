// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_TRANSACTION_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_TRANSACTION_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/completion_callback.h"
#include "net/base/load_states.h"
#include "net/base/request_priority.h"
#include "net/http/http_transaction.h"
#include "net/websockets/websocket_handshake_stream_base.h"

class DevToolsNetworkController;
class DevToolsNetworkInterceptor;
class GURL;

namespace net {
class AuthCredentials;
class BoundNetLog;
class HttpRequestHeaders;
struct HttpRequestInfo;
class HttpResponseInfo;
class HttpNetworkSession;
class IOBuffer;
struct LoadTimingInfo;
class UploadProgress;
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
class DevToolsNetworkTransaction : public net::HttpTransaction {
 public:
  DevToolsNetworkTransaction(
      DevToolsNetworkController* controller,
      scoped_ptr<net::HttpTransaction> network_transaction);

  virtual ~DevToolsNetworkTransaction();

  const net::HttpRequestInfo* request() const { return request_; }

  // Checks if request contains DevTools specific headers. Found values are
  // remembered and corresponding keys are removed from headers.
  void ProcessRequest();

  bool failed() const { return failed_; }

  // Runs callback (if any) with net::ERR_INTERNET_DISCONNECTED result value.
  void Fail();

  int64_t throttled_byte_count() const { return throttled_byte_count_; }
  void DecreaseThrottledByteCount(int64_t delta) {
    throttled_byte_count_ -= delta;
  }

  const std::string& request_initiator() const { return request_initiator_; }

  const std::string& client_id() const {
    return client_id_;
  }

  void FireThrottledCallback();

  // HttpTransaction methods:
  virtual int Start(
      const net::HttpRequestInfo* request,
      const net::CompletionCallback& callback,
      const net::BoundNetLog& net_log) OVERRIDE;
  virtual int RestartIgnoringLastError(
      const net::CompletionCallback& callback) OVERRIDE;
  virtual int RestartWithCertificate(
      net::X509Certificate* client_cert,
      const net::CompletionCallback& callback) OVERRIDE;
  virtual int RestartWithAuth(
      const net::AuthCredentials& credentials,
      const net::CompletionCallback& callback) OVERRIDE;
  virtual bool IsReadyToRestartForAuth() OVERRIDE;

  virtual int Read(
      net::IOBuffer* buf,
      int buf_len,
      const net::CompletionCallback& callback) OVERRIDE;
  virtual void StopCaching() OVERRIDE;
  virtual bool GetFullRequestHeaders(
      net::HttpRequestHeaders* headers) const OVERRIDE;
  virtual int64 GetTotalReceivedBytes() const OVERRIDE;
  virtual void DoneReading() OVERRIDE;
  virtual const net::HttpResponseInfo* GetResponseInfo() const OVERRIDE;
  virtual net::LoadState GetLoadState() const OVERRIDE;
  virtual net::UploadProgress GetUploadProgress() const OVERRIDE;
  virtual void SetQuicServerInfo(
      net::QuicServerInfo* quic_server_info) OVERRIDE;
  virtual bool GetLoadTimingInfo(
      net::LoadTimingInfo* load_timing_info) const OVERRIDE;
  virtual void SetPriority(net::RequestPriority priority) OVERRIDE;
  virtual void SetWebSocketHandshakeStreamCreateHelper(
      net::WebSocketHandshakeStreamBase::CreateHelper* create_helper) OVERRIDE;
  virtual void SetBeforeNetworkStartCallback(
      const BeforeNetworkStartCallback& callback) OVERRIDE;
  virtual void SetBeforeProxyHeadersSentCallback(
      const BeforeProxyHeadersSentCallback& callback) OVERRIDE;
  virtual int ResumeNetworkStart() OVERRIDE;

 protected:
  friend class test::DevToolsNetworkControllerHelper;

 private:
  // Proxy callback handler. Runs saved callback.
  void OnCallback(int result);

  DevToolsNetworkController* controller_;
  base::WeakPtr<DevToolsNetworkInterceptor> interceptor_;

  // Modified request. Should be destructed after |network_transaction_|
  scoped_ptr<net::HttpRequestInfo> custom_request_;

  // Real network transaction.
  scoped_ptr<net::HttpTransaction> network_transaction_;

  const net::HttpRequestInfo* request_;

  // True if Start was already invoked.
  bool started_;

  // True if Fail was already invoked.
  bool failed_;

  // Value of "X-DevTools-Request-Initiator" request header.
  std::string request_initiator_;

  // Value of "X-DevTools-Emulate-Network-Conditions-Client-Id" request header.
  std::string client_id_;

  enum CallbackType {
      NONE,
      READ,
      RESTART_IGNORING_LAST_ERROR,
      RESTART_WITH_AUTH,
      RESTART_WITH_CERTIFICATE,
      START
  };

  int SetupCallback(
      net::CompletionCallback callback,
      int result,
      CallbackType callback_type);

  void Throttle(int result);

  int throttled_result_;
  int64_t throttled_byte_count_;
  CallbackType callback_type_;
  net::CompletionCallback proxy_callback_;
  net::CompletionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsNetworkTransaction);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_TRANSACTION_H_
